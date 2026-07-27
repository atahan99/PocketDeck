#include <Preferences.h>

#include "apps.h"
#include "ui.h"

using ui::fb;
using ui::frame;

// Easter-egg arcade. Six small games behind a submenu. Most use the two buttons
// so they're fully drivable headless; Tilt Maze uses the BMI270 (its whole
// point). Dino keeps a high score in NVS.
//
// ponytail: the IMU tilt sign is a hardware calibration knob -- which way "left"
// reads depends on how you hold it. TILT_SIGN flips it if a tilt game feels
// mirrored. Can't be auto-derived without knowing the grip.
namespace {
constexpr float TILT_SIGN = 1.0f;

// ---- shared helpers ----------------------------------------------------
int center_msg(const char *l1, const char *l2) {  // game-over/intro screen
  fb.setTextDatum(middle_center);
  fb.setTextColor(TFT_WHITE, TFT_BLACK);
  fb.setTextSize(2);
  fb.drawString(l1, ui::W / 2, ui::H / 2 - 12);
  fb.setTextSize(1);
  if (l2) fb.drawString(l2, ui::W / 2, ui::H / 2 + 14);
  fb.setTextDatum(top_left);
  return 0;
}

// ---- Snake -------------------------------------------------------------
void game_snake() {
  const int CS = 8, cols = ui::W / CS, rows = (ui::H - 16) / CS, y0 = 16;
  int sx[256], sy[256], len, dir;
  int fx, fy;
  auto reset = [&]() {
    len = 3; dir = 0;                       // 0=R,1=D,2=L,3=U
    for (int i = 0; i < len; i++) { sx[i] = cols / 2 - i; sy[i] = rows / 2; }
    fx = random(cols); fy = random(rows);
  };
  reset();
  uint32_t step = 0;
  bool dead = false;
  while (true) {
    ui::Ev ev = frame(33);  // ~30fps: physics is per-frame, so the loop must be capped
    if (ev.home) return;
    if (dead) { if (ev.a) { reset(); dead = false; } continue; }
    if (ev.a) dir = (dir + 3) & 3;          // turn left
    if (ev.b) dir = (dir + 1) & 3;          // turn right

    if (millis() - step > 130) {
      step = millis();
      int nx = sx[0] + (dir == 0) - (dir == 2);
      int ny = sy[0] + (dir == 1) - (dir == 3);
      if (nx < 0 || ny < 0 || nx >= cols || ny >= rows) { dead = true; }
      for (int i = 0; i < len && !dead; i++)
        if (sx[i] == nx && sy[i] == ny) dead = true;
      if (!dead) {
        if (nx == fx && ny == fy) { if (len < 255) len++; fx = random(cols); fy = random(rows); }
        for (int i = len - 1; i > 0; i--) { sx[i] = sx[i - 1]; sy[i] = sy[i - 1]; }
        sx[0] = nx; sy[0] = ny;
      }
    }

    ui::header("Snake", TFT_GREEN);
    fb.setCursor(180, 4); fb.setTextColor(TFT_DARKGREY, TFT_BLACK); fb.printf("len %d", len);
    fb.fillRect(fx * CS, y0 + fy * CS, CS - 1, CS - 1, TFT_RED);
    for (int i = 0; i < len; i++)
      fb.fillRect(sx[i] * CS, y0 + sy[i] * CS, CS - 1, CS - 1, i ? TFT_GREEN : TFT_WHITE);
    if (dead) center_msg("GAME OVER", "A = retry");
  }
}

// ---- Flappy ------------------------------------------------------------
void game_flappy() {
  float by, vy; int px, gap; bool dead; int score;
  auto reset = [&]() { by = ui::H / 2; vy = 0; px = ui::W; gap = 40; dead = false; score = 0; };
  reset();
  while (true) {
    ui::Ev ev = frame(33);  // ~30fps: physics is per-frame, so the loop must be capped
    if (ev.home) return;
    if (dead) { if (ev.a) reset(); }
    else if (ev.a) vy = -3.2f;

    if (!dead) {
      vy += 0.25f; by += vy;
      px -= 3;
      if (px < -12) { px = ui::W; gap = 30 + random(ui::H - 80); score++; }
      if (by < 14 || by > ui::H - 6) dead = true;
      if (px < 30 && px > 6 && (by < gap || by > gap + 46)) dead = true;
    }
    ui::header("Flappy", TFT_YELLOW);
    fb.setCursor(190, 4); fb.setTextColor(TFT_DARKGREY, TFT_BLACK); fb.printf("%d", score);
    fb.fillRect(px, 14, 12, gap - 14, TFT_GREEN);
    fb.fillRect(px, gap + 46, 12, ui::H - (gap + 46), TFT_GREEN);
    fb.fillCircle(24, (int)by, 5, TFT_YELLOW);
    if (dead) center_msg("GAME OVER", "A = retry");
  }
}

// ---- Dino --------------------------------------------------------------
void game_dino() {
  Preferences p; p.begin("toys", false);
  int hi = p.getInt("dino", 0);
  float y, vy; bool dead; int cx, score; uint32_t t0;
  auto reset = [&]() { y = 0; vy = 0; dead = false; cx = ui::W; score = 0; t0 = millis(); };
  reset();
  const int ground = ui::H - 14;
  while (true) {
    ui::Ev ev = frame(33);  // ~30fps: physics is per-frame, so the loop must be capped
    if (ev.home) { p.putInt("dino", hi); p.end(); return; }
    if (dead) { if (ev.a) reset(); }
    else if (ev.a && y == 0) vy = -4.6f;

    if (!dead) {
      vy += 0.30f; y += vy; if (y > 0) { y = 0; vy = 0; }
      int spd = 4 + (millis() - t0) / 8000;
      cx -= spd;
      if (cx < -10) { cx = ui::W + random(60); }
      score = (millis() - t0) / 100;
      if (score > hi) hi = score;
      if (cx > 16 && cx < 34 && y > -12) dead = true;   // hit cactus
    }
    ui::header("Dino", TFT_WHITE);
    fb.setCursor(150, 4); fb.setTextColor(TFT_DARKGREY, TFT_BLACK); fb.printf("%d hi %d", score, hi);
    fb.drawFastHLine(0, ground + 8, ui::W, TFT_DARKGREY);
    fb.fillRect(20, ground + (int)y - 8, 10, 12, TFT_WHITE);        // dino
    fb.fillRect(cx, ground - 2, 8, 12, TFT_GREEN);                  // cactus
    if (dead) center_msg("GAME OVER", "A = retry");
  }
}

// ---- Breakout ----------------------------------------------------------
void game_breakout() {
  const int cols = 8, brow = 3;
  bool brick[cols * brow];
  float bx, by, vx, vy; int px; bool dead, win;
  auto reset = [&]() {
    for (auto &b : brick) b = true;
    bx = ui::W / 2; by = ui::H / 2; vx = 1.6f; vy = 2.0f; px = ui::W / 2; dead = win = false;
  };
  reset();
  const int pw = 34, py = ui::H - 8, bw = ui::W / cols, bh = 8;
  while (true) {
    ui::Ev ev = frame(33);  // ~30fps: physics is per-frame, so the loop must be capped
    if (ev.home) return;
    if (dead || win) { if (ev.a) reset(); }
    if (ev.a) px -= 12;
    if (ev.b) px += 12;
    px = constrain(px, pw / 2, ui::W - pw / 2);

    if (!dead && !win) {
      bx += vx; by += vy;
      if (bx < 2 || bx > ui::W - 2) vx = -vx;
      if (by < 16) vy = -vy;
      if (by > py - 4 && by < py + 2 && bx > px - pw / 2 && bx < px + pw / 2) { vy = -fabs(vy); vx += (bx - px) * 0.04f; }
      if (by > ui::H) dead = true;
      int c = (int)bx / bw, r = ((int)by - 16) / bh;
      if (r >= 0 && r < brow && c >= 0 && c < cols && brick[r * cols + c]) {
        brick[r * cols + c] = false; vy = -vy;
        win = true; for (auto b : brick) if (b) win = false;
      }
    }
    ui::header("Breakout", TFT_CYAN);
    for (int r = 0; r < brow; r++)
      for (int c = 0; c < cols; c++)
        if (brick[r * cols + c])
          fb.fillRect(c * bw + 1, 16 + r * bh, bw - 2, bh - 1, r == 0 ? TFT_RED : r == 1 ? TFT_ORANGE : TFT_GREEN);
    fb.fillRect(px - pw / 2, py, pw, 4, TFT_WHITE);
    fb.fillCircle((int)bx, (int)by, 3, TFT_CYAN);
    if (dead) center_msg("GAME OVER", "A = retry");
    if (win) center_msg("YOU WIN", "A = again");
  }
}

// ---- Tilt Maze ---------------------------------------------------------
void game_maze() {
  // Fixed 15x8 maze, 1=wall. Roll the ball to the green exit by tilting.
  static const char *M[8] = {
      "###############", "#S..#.....#...#", "#.#.#.###.#.#.#",
      "#.#...#...#.#.#", "#.#####.###.#.#", "#.....#...#...#",
      "#.###.#.#####.E", "###############"};
  const int cw = ui::W / 15, ch = (ui::H - 14) / 8, y0 = 14;
  float bx = 1.5f, by = 1.5f, vx = 0, vy = 0;
  auto wall = [&](float x, float y) {
    int c = (int)x, r = (int)y;
    return r < 0 || r >= 8 || c < 0 || c >= 15 || M[r][c] == '#';
  };
  while (true) {
    ui::Ev ev = frame(33);  // ~30fps: physics is per-frame, so the loop must be capped
    if (ev.home) return;
    float ax, ay, az; M5.Imu.getAccel(&ax, &ay, &az);
    vx += TILT_SIGN * ay * 0.06f; vy += -ax * 0.06f;
    vx *= 0.9f; vy *= 0.9f;
    float nx = bx + vx, ny = by + vy;
    if (!wall(nx, by)) bx = nx; else vx = 0;
    if (!wall(bx, ny)) by = ny; else vy = 0;

    ui::header("Tilt Maze", TFT_MAGENTA);
    for (int r = 0; r < 8; r++)
      for (int c = 0; c < 15; c++) {
        char t = M[r][c];
        if (t == '#') fb.fillRect(c * cw, y0 + r * ch, cw, ch, TFT_BLUE);
        else if (t == 'E') fb.fillRect(c * cw, y0 + r * ch, cw, ch, TFT_GREEN);
      }
    fb.fillCircle((int)(bx * cw), y0 + (int)(by * ch), cw / 2 - 1, TFT_WHITE);
    if (M[(int)by][(int)bx] == 'E') { center_msg("SOLVED!", "long-press = exit"); }
  }
}

// ---- Tetris (portrait) -------------------------------------------------
void game_tetris() {
  ui::orient(false);                          // 135x240 tall well
  const int CW = 12, cols = 10, rows = 18, x0 = 7, y0 = 16;
  static const uint16_t SH[7] = {0x0F00, 0x2222, 0x0660, 0x0C60, 0x06C0, 0x4460, 0x2260};
  static const uint16_t COL[7] = {TFT_CYAN, TFT_YELLOW, TFT_GREEN, TFT_RED, TFT_MAGENTA, TFT_BLUE, TFT_ORANGE};
  uint8_t grid[rows][cols];
  int px, py, pr, pk; bool dead;
  auto cell = [&](int k, int r, int i, int j) {
    uint16_t s = SH[k];
    for (int b = 0; b < r; b++) {             // rotate by transposing bits
      uint16_t o = 0;
      for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++)
        if (s & (1 << (y * 4 + x))) o |= 1 << (x * 4 + (3 - y));
      s = o;
    }
    return (bool)(s & (1 << ((i) * 4 + (j))));
  };
  auto hit = [&](int nx, int ny, int nr) {
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
      if (cell(pk, nr, i, j)) {
        int gx = nx + j, gy = ny + i;
        if (gx < 0 || gx >= cols || gy >= rows) return true;
        if (gy >= 0 && grid[gy][gx]) return true;
      }
    return false;
  };
  auto spawn = [&]() { px = 3; py = -1; pr = 0; pk = random(7); if (hit(px, py, pr)) dead = true; };
  memset(grid, 0, sizeof(grid));
  dead = false; spawn();
  uint32_t fall = millis();
  while (true) {
    ui::Ev ev = frame(33);  // ~30fps: physics is per-frame, so the loop must be capped
    if (ev.home) { ui::orient(true); return; }
    if (dead) { if (ev.a) { memset(grid, 0, sizeof(grid)); dead = false; spawn(); } }
    else {
      if (ev.a && !hit(px, py, (pr + 1) & 3)) pr = (pr + 1) & 3;     // rotate
      if (ev.b) {                                                    // move right, wrap at wall
        if (!hit(px + 1, py, pr)) px++;
        else while (!hit(px - 1, py, pr)) px--;                      // snap to far left -> all columns reachable
      }
      bool drop = millis() - fall > 500;
      if (drop) {
        fall = millis();
        if (!hit(px, py + 1, pr)) py++;
        else {
          for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
            if (cell(pk, pr, i, j) && py + i >= 0) grid[py + i][px + j] = pk + 1;
          for (int r = rows - 1; r >= 0; r--) {                       // clear full lines
            bool full = true; for (int c = 0; c < cols; c++) if (!grid[r][c]) full = false;
            if (full) { for (int rr = r; rr > 0; rr--) memcpy(grid[rr], grid[rr - 1], cols); memset(grid[0], 0, cols); r++; }
          }
          spawn();
        }
      }
    }
    ui::header("Tetris", TFT_CYAN);
    fb.drawRect(x0 - 1, y0 - 1, cols * CW + 2, rows * CW + 2, TFT_DARKGREY);
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++)
      if (grid[r][c]) fb.fillRect(x0 + c * CW, y0 + r * CW, CW - 1, CW - 1, COL[grid[r][c] - 1]);
    if (!dead) for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
      if (cell(pk, pr, i, j) && py + i >= 0)
        fb.fillRect(x0 + (px + j) * CW, y0 + (py + i) * CW, CW - 1, CW - 1, COL[pk]);
    if (dead) center_msg("GAME OVER", "A = retry");
  }
}

struct Game { const char *name; void (*run)(); };
const Game GAMES[] = {
    {"Tetris", game_tetris}, {"Snake", game_snake}, {"Flappy", game_flappy},
    {"Dino", game_dino}, {"Breakout", game_breakout}, {"Tilt Maze", game_maze},
};
const int NG = sizeof(GAMES) / sizeof(GAMES[0]);
}  // namespace

void app_toys() {
  randomSeed(esp_random());
  int sel = 0;
  while (true) {
    ui::header("Toys", TFT_GREEN);
    fb.setTextSize(2);
    const int cw = ui::W / 2, ch = 30;
    for (int i = 0; i < NG; i++) {
      int x = (i % 2) * cw, y = 20 + (i / 2) * ch;
      bool on = i == sel;
      if (on) fb.fillRoundRect(x + 2, y, cw - 4, ch - 4, 4, TFT_DARKGREEN);
      fb.setTextColor(on ? TFT_WHITE : TFT_GREEN, on ? TFT_DARKGREEN : TFT_BLACK);
      fb.setCursor(x + 6, y + 5);
      fb.print(GAMES[i].name);
    }
    fb.setTextSize(1);
    ui::Ev ev = frame();
    if (ev.home) return;
    if (ev.b) sel = (sel + 1) % NG;
    if (ev.a) GAMES[sel].run();
  }
}
