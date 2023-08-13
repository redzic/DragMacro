#include <Windows.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <span>
#include <tuple>
#include <vector>

constexpr auto NUM_POINTS = 2500;

constexpr int CLICK = -1;
constexpr int UNCLICK = -2;

// click        = -1, <any>
// unclick      = -2, <any>
// mouse move   = <x>0>, <y>0>

void SimulateLeftMouseDown() {
  INPUT input = {0};
  input.type = INPUT_MOUSE;
  input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
  SendInput(1, &input, sizeof(INPUT));
}

void SimulateLeftMouseUp() {
  INPUT input = {0};
  input.type = INPUT_MOUSE;
  input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
  SendInput(1, &input, sizeof(INPUT));
}

void ReplayMacro(std::span<std::tuple<int, int>> cursor_positions) {
  int mouse_moves = 0;

  for (int i = 0; i < cursor_positions.size(); ++i) {
    auto pos = cursor_positions[i];
    auto [x, y] = pos;

    if (x == CLICK) {
      SimulateLeftMouseDown();
    } else if (x == UNCLICK) {
      SimulateLeftMouseUp();
    } else {
      SetCursorPos(x, y);
      mouse_moves++;
    }

    // The frequency of when the delay happens seems to affect
    // the smoothness of the lines in gartic phone
    // bro and for some reason if you skip too many things
    // it just doesn't draw the entire thing
    // even if it appears correctly on the preview and shit
    // if (mouse_moves % 10 == 0) {
    Sleep(1);
    // }
  }
}

int SerializeMacro(const std::span<std::tuple<int, int>> data,
                   const char *filename) {
  std::ofstream outFile(filename, std::ios::binary);
  if (!outFile.is_open()) {
    std::cerr << "Failed to open file for writing.\n";
    return -1;
  }

  size_t size = data.size();
  outFile.write(reinterpret_cast<const char *>(&size), sizeof(size));

  for (const auto &tuple : data) {
    auto [first, second] = tuple;
    outFile.write(reinterpret_cast<const char *>(&first), sizeof(first));
    outFile.write(reinterpret_cast<const char *>(&second), sizeof(second));
  }

  outFile.close();
  return 0;
}

// Deserialize a binary file to vector of tuples
std::vector<std::tuple<int, int>> DeserializeMacro(const char *filename) {
  std::vector<std::tuple<int, int>> result{};

  std::ifstream inFile(filename, std::ios::binary);
  if (!inFile.is_open()) {
    std::cerr << "Failed to open file for reading.\n";
    return result;
  }

  size_t size;
  inFile.read(reinterpret_cast<char *>(&size), sizeof(size));

  for (size_t i = 0; i < size; ++i) {
    int first;
    int second;
    inFile.read(reinterpret_cast<char *>(&first), sizeof(first));
    inFile.read(reinterpret_cast<char *>(&second), sizeof(second));
    result.emplace_back(first, second);
  }

  inFile.close();
  return result;
}

void RecordMacroFile(const char *filename) {
  std::vector<std::tuple<int, int>> cursor_positions = {};
  cursor_positions.reserve(NUM_POINTS + 64);

  auto get_leftclick_state = []() {
    return (GetKeyState(VK_LBUTTON) & 0x8000) != 0;
  };
  auto is_key_pressed = [](int virtKey) {
    return (GetKeyState(virtKey) & 0x8000) != 0;
  };

  POINT point;

  int last_x = -1;
  int last_y = -1;
  bool last_pressed = false;

  // gonna have to be very careful how you code the check if it's a new mouse
  // point

  int num_mouse_moves = 0;
  // how many moves and clicks are deleted by undo
  int minus_undo = 0;

  //   because it's allowed to record a new mouse event for unclick and reclick
  // at same position
  while (!is_key_pressed('Q')) {
    // BUT... this doesn't handle just unclicking mouse
    // without moving...

    // if Ctrl+Z
    if (is_key_pressed(VK_CONTROL) && is_key_pressed('Z')) {
      // delete until Click, and if there's a move before that, delete it
      // assert(!cursor_positions.empty());
      if (!cursor_positions.empty()) {
        while (!cursor_positions.empty()) {
          auto [cx, cy] = cursor_positions[cursor_positions.size() - 1];
          cursor_positions.pop_back();
          minus_undo++;
          if (cx == CLICK) {
            if (!cursor_positions.empty()) {
              auto [cx2, cy2] = cursor_positions[cursor_positions.size() - 1];

              // assert it's a move
              assert(cx2 >= 0 && cy2 >= 0);
              cursor_positions.pop_back();
              minus_undo++;
            }
            break;
          }
        }
      }

      // (X) = X is optional
      // Click, (Move), Unclick, (Move), Click, (Move), Unclick, (Move), ...
      //    ... Unclick

      // wait until only Z is released to match behavior of gartic phone
      while (is_key_pressed('Z')) {
      }
      continue;
    }

    bool mouse_pressed = get_leftclick_state();
    GetCursorPos(&point);

    if (mouse_pressed) {
      if (last_pressed) {
        if (point.x != last_x || point.y != last_y) {
          cursor_positions.emplace_back(point.x, point.y);

          num_mouse_moves++;
        }
        // else ignore input if it's the same coordinate
      } else {
        // unconditionally record mouse pos and click
        // unclick
        cursor_positions.emplace_back(UNCLICK, 0);
        // move mouse
        cursor_positions.emplace_back(point.x, point.y);
        // click
        cursor_positions.emplace_back(CLICK, 0);

        num_mouse_moves++;
      }
      last_pressed = true;

      // std::cout << "(" << point.x << ", " << point.y << ")\n";
      // std::cout << "mouse pressed? " << mouse_pressed << '\n';

    } else {
      last_pressed = false;
    }
    last_x = point.x;
    last_y = point.y;
  }
  cursor_positions.emplace_back(UNCLICK, 0);

  // num_mouse_moves is only used for printing out purposes
  printf("num mouse moves (including undo): %d\n", num_mouse_moves);
  printf("num mouse moves (  without undo): %d\n",
         num_mouse_moves - minus_undo);

  SerializeMacro(cursor_positions, filename);
  // TODO error handling
  printf("Successfully saved macro to '%s'\n", filename);
}

void ReplayMacroFile(const char *filename) {
  auto m_moves = DeserializeMacro(filename);
  ReplayMacro(m_moves);
  printf("Successfully replayed macro file with size of %lld\n",
         m_moves.size());
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cout << "Incorrect number of arguments specified\n"
                 "Usage: DragMacro  <record/replay>  <macro_file>\n";
  }

  // TODO add confirmation before overwriting file in record macro
  // TODO add append macro mode

  // TODO there might be a bug where you right click while dragging the mode

  std::string_view cmd{argv[1]};
  if (cmd == "record") {
    RecordMacroFile(argv[2]);
  } else if (cmd == "replay") {
    Sleep(5000);
    ReplayMacroFile(argv[2]);
  } else {
    std::cout << "Unknown subcommand, use either 'record' or 'replay'\n";
    return 0;
  }

  return 0;
}
