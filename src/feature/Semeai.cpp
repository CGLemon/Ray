#include <memory>

#include "board/GoBoard.hpp"
#include "board/Point.hpp"
#include "board/SearchBoard.hpp"
#include "common/Message.hpp"
#include "feature/Semeai.hpp"
#include "pattern/Pattern.hpp"
#include "mcts/UctRating.hpp"


/////////////////////////
//  1手で取れるか確認  //
/////////////////////////
bool
IsCapturableAtari( const game_info_t *game, const int pos, const int color, const int opponent_pos )
{
  const int other = GetOppositeColor(color);

  if (!IsLegal(game, pos, color)) {
    return false;
  }

  std::unique_ptr<search_game_info_t> search_game(new search_game_info_t(game));
  search_game_info_t *capturable_game = search_game.get();

  // とりあえず石を置く
  PutStoneForSearch(capturable_game, pos, color);

  const string_t *string = capturable_game->string;
  const int *string_id = capturable_game->string_id;
  const int id = string_id[opponent_pos];

  // 周囲に取り返せる石があれば安全
  int neighbor = string[id].neighbor[0];
  while (neighbor != NEIGHBOR_END) {
    if (string[neighbor].libs == 1) {
      return false;
    }
    neighbor = string[id].neighbor[neighbor];
  }

  if (!IsLegalForSearch(capturable_game, string[string_id[opponent_pos]].lib[0], other)) {
    return true;
  }
  // 逃げるつもりでダメに打つ
  PutStoneForSearch(capturable_game, string[string_id[opponent_pos]].lib[0], other);

  // 逃げても呼吸点が1つなら捕獲可能と判定
  if (string[string_id[opponent_pos]].libs == 1) {
    return true;
  } else {
    return false;
  }
}


////////////////////////////////
//  オイオトシかどうかを確認  //
////////////////////////////////
// 返り値がintとboolの違いだけでIsCapturableAtari関数と同じ
int
CheckOiotoshi( const game_info_t *game, const int pos, const int color, const int opponent_pos )
{
  const int other = GetOppositeColor(color);
  int num = -1;

  if (!IsLegal(game, pos, color)) {
    return -1;
  }

  std::unique_ptr<search_game_info_t> search_game(new search_game_info_t(game));
  search_game_info_t *oiotoshi_game = search_game.get();
  
  PutStoneForSearch(oiotoshi_game, pos, color);

  const string_t *string = oiotoshi_game->string;
  const int *string_id = oiotoshi_game->string_id;
  const int id = string_id[opponent_pos];

  int neighbor = string[id].neighbor[0];
  while (neighbor != NEIGHBOR_END) {
    if (string[neighbor].libs == 1) {
      return -1;
    }
    neighbor = string[id].neighbor[neighbor];
  }

  if (!IsLegalForSearch(oiotoshi_game, string[string_id[opponent_pos]].lib[0], other)) {
    return -1;
  }
  PutStoneForSearch(oiotoshi_game, string[string_id[opponent_pos]].lib[0], other);

  if (string[string_id[opponent_pos]].libs == 1) {
    num = string[string_id[opponent_pos]].size;
  }

  return num;
}


//////////////////////////////////////////////
//  石をすぐに捕獲できそうな候補手を求める  //
//////////////////////////////////////////////
int
CapturableCandidate( const game_info_t *game, const int id )
{
  const string_t *string = game->string;
  int neighbor = string[id].neighbor[0];
  bool flag = false;
  int capturable_pos = -1;

  // 隣接する呼吸点が1つの敵連が1つだけの時, 候補を返す
  while (neighbor != NEIGHBOR_END) {
    if (string[neighbor].libs == 1) {
      if (string[neighbor].size >= 2) {
        return -1;
      } else {
        if (flag) {
          return -1;
        }
        capturable_pos = string[neighbor].lib[0];
        flag = true;
      }
    }
    neighbor = string[id].neighbor[neighbor];
  }

  return capturable_pos;
}


////////////////////////////////////
//  すぐに捕まる手かどうかを判定  //
////////////////////////////////////
bool
IsDeadlyExtension( const game_info_t *game, const int color, const int id )
{
  const int other = GetOppositeColor(color);
  int pos = game->string[id].lib[0];

  if (nb4_empty[Pat3(game->pat, pos)] == 0 &&
      IsSuicide(game, game->string, other, pos)) {
    return true;
  }

  std::unique_ptr<search_game_info_t> search_game_data(new search_game_info_t(game));
  search_game_info_t *search_game = search_game_data.get();

  PutStoneForSearch(search_game, pos, other);

  if (search_game->string[search_game->string_id[pos]].libs == 1) {
    return true;
  } else {
    return false;
  }
}


/////////////////////////////////
//  自己アタリになるトリか判定  //
/////////////////////////////////
bool
IsSelfAtariCapture( const game_info_t *game, const int pos, const int color, const int id )
{

  if (!IsLegal(game, pos, color)) {
    return false;
  }

  std::unique_ptr<search_game_info_t> search_game(new search_game_info_t(game));
  search_game_info_t *capture_game = search_game.get();

  PutStoneForSearch(capture_game, pos, color);

  const string_t *string = capture_game->string;
  const int *string_id = capture_game->string_id;
  const int string_pos = game->string[id].origin;

  if (string[string_id[string_pos]].libs == 1) {
    return true;
  } else {
    return false;
  }
}

////////////////////////////////////////
//  呼吸点がどのように変化するかを確認  //
////////////////////////////////////////
int
CheckLibertyState( const game_info_t *game, const int pos, const int color, const int id )
{
  const int string_pos = game->string[id].origin;
  const int libs = game->string[id].libs;

  if (!IsLegal(game, pos, color)) {
    return L_DECREASE;
  }

  std::unique_ptr<search_game_info_t> search_game(new search_game_info_t(game));
  search_game_info_t *liberty_game = search_game.get();

  PutStoneForSearch(liberty_game, pos, color);

  const string_t *string = liberty_game->string;
  const int *string_id = liberty_game->string_id;
  const int new_libs = string[string_id[string_pos]].libs;

  if (new_libs > libs + 1) {
    return L_INCREASE;
  } else if (new_libs > libs) {
    return L_EVEN;
  } else {
    return L_DECREASE;
  }
}


///////////////////////////////////////////////
//  1手で取れるかを判定(シミュレーション用)  //
///////////////////////////////////////////////
bool
IsCapturableAtariForSimulation( const game_info_t *game, const int pos, const int color, const int id )
{
  const char *board = game->board;
  const string_t *string = game->string;
  const int *string_id = game->string_id;
  const int other = GetOppositeColor(color);
  int lib;
  bool neighbor = false;
  int index_distance;
  int pat3;
  int empty;
  int connect_libs = 0;
  int tmp_id;

  lib = string[id].lib[0];

  if (lib == pos) {
    lib = string[id].lib[lib];
  }

  index_distance = lib - pos;

  // 詰める方とは逆のダメの周囲の空点数を調べる
  pat3 = Pat3(game->pat, lib);
  empty = nb4_empty[pat3];

  // 逆のダメの周囲の空点数が3なら取れないのでfalse
  if (empty == 3) {
    return false;
  }

  if (index_distance ==           1) neighbor = true;
  if (index_distance ==          -1) neighbor = true;
  if (index_distance ==  board_size) neighbor = true;
  if (index_distance == -board_size) neighbor = true;

  // ダメが隣り合っている時と
  // ダメが離れている時の分岐
  if (( neighbor && empty >= 3) ||
      (!neighbor && empty >= 2)) {
    return false;
  }

  // 隣接する連がlib以外に持つ呼吸点の合計数が
  // 2以上なら無条件で1手で取れるアタリではないのでfalse

  // 上の確認
  if (board[NORTH(lib)] == other && 
      string_id[NORTH(lib)] != id) {
    tmp_id = string_id[NORTH(lib)];
    if (string[tmp_id].libs > 2) {
      return false;
    } else {
      connect_libs += string[tmp_id].libs - 1;
    }
  } 

  // 左の確認
  if (board[WEST(lib)] == other && 
      string_id[WEST(lib)] != id) {
    tmp_id = string_id[WEST(lib)];
    if (string[tmp_id].libs > 2) {
      return false;
    } else {
      connect_libs += string[tmp_id].libs - 1;
    }
  }

  // 右の確認
  if (board[EAST(lib)] == other && 
      string_id[EAST(lib)] != id) {
    tmp_id = string_id[EAST(lib)];
    if (string[tmp_id].libs > 2) {
      return false;
    } else {
      connect_libs += string[tmp_id].libs - 1;
    }
  }

  // 下の確認
  if (board[SOUTH(lib)] == other && 
      string_id[SOUTH(lib)] != id) {
    tmp_id = string_id[SOUTH(lib)];
    if (string[tmp_id].libs > 2) {
      return false;
    } else {
      connect_libs += string[tmp_id].libs - 1;
    }
  }

  // ダメに打っても増える呼吸点数が1以下なら
  // 1手で取れるアタリ
  if (( neighbor && connect_libs < 2) ||
      (!neighbor && connect_libs < 1)) {
    return true;
  } else {
    return false;
  }
}


bool
IsSelfAtariCaptureForSimulation( const game_info_t *game, const int pos, const int color, const int lib )
{
  const char *board = game->board;
  const string_t *string = game->string;
  const int *string_id = game->string_id;
  const int other = GetOppositeColor(color);
  int id;
  int size = 0;

  if (lib != pos || 
      nb4_empty[Pat3(game->pat, pos)] != 0) {
    return false;
  }

  if (board[NORTH(pos)] == color) {
    id = string_id[NORTH(pos)];
    if (string[id].libs > 1) {
      return false;
    }
  } else if (board[NORTH(pos)] == other) {
    id = string_id[NORTH(pos)];
    size += string[id].size;
    if (size > 1) {
      return false;
    }
  }

  if (board[WEST(pos)] == color) {
    id = string_id[WEST(pos)];
    if (string[id].libs > 1) {
      return false;
    }
  } else if (board[WEST(pos)] == other) {
    id = string_id[WEST(pos)];
    size += string[id].size;
    if (size > 1) {
      return false;
    }
  }

  if (board[EAST(pos)] == color) {
    id = string_id[EAST(pos)];
    if (string[id].libs > 1) {
      return false;
    }
  } else if (board[EAST(pos)] == other) {
    id = string_id[EAST(pos)];
    size += string[id].size;
    if (size > 1) {
      return false;
    }
  }

  if (board[SOUTH(pos)] == color) {
    id = string_id[SOUTH(pos)];
    if (string[id].libs > 1) {
      return false;
    }
  } else if (board[SOUTH(pos)] == other) {
    id = string_id[SOUTH(pos)];
    size += string[id].size;
    if (size > 1) {
      return false;
    }
  }

  return true;
}

bool
IsSelfAtari( const game_info_t *game, const int color, const int pos )
{
  const char *board = game->board;
  const string_t *string = game->string;
  const int *string_id = game->string_id;
  const int other = GetOppositeColor(color);
  int already[4] = { 0 };
  int already_num = 0;
  int lib, count = 0, libs = 0;
  int lib_candidate[10];
  int i;
  int id;
  bool checked;

  // 上下左右が空点なら呼吸点の候補に入れる
  if (board[NORTH(pos)] == S_EMPTY) lib_candidate[libs++] = NORTH(pos);
  if (board[ WEST(pos)] == S_EMPTY) lib_candidate[libs++] =  WEST(pos);
  if (board[ EAST(pos)] == S_EMPTY) lib_candidate[libs++] =  EAST(pos);
  if (board[SOUTH(pos)] == S_EMPTY) lib_candidate[libs++] = SOUTH(pos);

  //  空点
  if (libs >= 2) return false;

  // 上を調べる
  if (board[NORTH(pos)] == color) {
    id = string_id[NORTH(pos)];
    if (string[id].libs > 2) return false;
    lib = string[id].lib[0];
    count = 0;
    while (lib != LIBERTY_END) {
      if (lib != pos) {
        checked = false;
        for (i = 0; i < libs; i++) {
          if (lib_candidate[i] == lib) {
            checked = true;
            break;
          }
        }
        if (!checked) {
          lib_candidate[libs + count] = lib;
          count++;
        }
      }
      lib = string[id].lib[lib];
    }
    libs += count;
    already[already_num++] = id;
    if (libs >= 2) return false;
  } else if (board[NORTH(pos)] == other &&
             string[string_id[NORTH(pos)]].libs == 1) {
    return false;
  }

  // 左を調べる
  if (board[WEST(pos)] == color) {
    id = string_id[WEST(pos)];
    if (already[0] != id) {
      if (string[id].libs > 2) return false;
      lib = string[id].lib[0];
      count = 0;
      while (lib != LIBERTY_END) {
        if (lib != pos) {
          checked = false;
          for (i = 0; i < libs; i++) {
            if (lib_candidate[i] == lib) {
              checked = true;
              break;
            }
          }
          if (!checked) {
            lib_candidate[libs + count] = lib;
            count++;
          }
        }
        lib = string[id].lib[lib];
      }
      libs += count;
      already[already_num++] = id;
      if (libs >= 2) return false;
    }
  } else if (board[WEST(pos)] == other &&
             string[string_id[WEST(pos)]].libs == 1) {
    return false;
  }

  // 右を調べる
  if (board[EAST(pos)] == color) {
    id = string_id[EAST(pos)];
    if (already[0] != id && already[1] != id) {
      if (string[id].libs > 2) return false;
      lib = string[id].lib[0];
      count = 0;
      while (lib != LIBERTY_END) {
        if (lib != pos) {
          checked = false;
          for (i = 0; i < libs; i++) {
            if (lib_candidate[i] == lib) {
              checked = true;
              break;
            }
          }
          if (!checked) {
            lib_candidate[libs + count] = lib;
            count++;
          }
        }
        lib = string[id].lib[lib];
      }
      libs += count;
      already[already_num++] = id;
      if (libs >= 2) return false;
    }
  } else if (board[EAST(pos)] == other &&
             string[string_id[EAST(pos)]].libs == 1) {
    return false;
  }


  // 下を調べる
  if (board[SOUTH(pos)] == color) {
    id = string_id[SOUTH(pos)];
    if (already[0] != id && already[1] != id && already[2] != id) {
      if (string[id].libs > 2) return false;
      lib = string[id].lib[0];
      count = 0;
      while (lib != LIBERTY_END) {
        if (lib != pos) {
          checked = false;
          for (i = 0; i < libs; i++) {
            if (lib_candidate[i] == lib) {
              checked = true;
              break;
            }
          }
          if (!checked) {
            lib_candidate[libs + count] = lib;
            count++;
          }
        }
        lib = string[id].lib[lib];
      }
      libs += count;
      already[already_num++] = id;
      if (libs >= 2) return false;
    }
  } else if (board[SOUTH(pos)] == other &&
             string[string_id[SOUTH(pos)]].libs == 1) {
    return false;
  }

  return true;
}


bool 
IsAlreadyCaptured( const game_info_t *game, const int id, int player_id[], int player_ids )
{
  const string_t *string = game->string;
  const int *string_id = game->string_id;
  int lib1, lib2;
  bool checked;
  int neighbor4[4];
  int i, j;

  if (string[id].libs == 1) {
    return true;
  } else if (string[id].libs == 2) {
    lib1 = string[id].lib[0];
    lib2 = string[id].lib[lib1];

    GetNeighbor4(neighbor4, lib1);
    checked = false;
    for (i = 0; i < 4; i++) {
      for (j = 0; j < player_ids; j++) {
        if (player_id[j] == string_id[neighbor4[i]]) {
          checked = true;
          player_id[j] = 0;
        }
      }
    }
    if (checked == false) return false;

    GetNeighbor4(neighbor4, lib2);
    checked = false;
    for (i = 0; i < 4; i++) {
      for (j = 0; j < player_ids; j++) {
        if (player_id[j] == string_id[neighbor4[i]]) {
          checked = true;
          player_id[j] = 0;
        }
      }
    }
    if (checked == false) return false;

    for (i = 0; i < player_ids; i++) {
      if (player_id[i] != 0) {
        return false;
      }
    }

    return true;
  } else {
    return false;
  }
}
