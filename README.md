# 🗡️ Terminal & 2D GUI Roguelike Game Engine

Một dự án game Roguelike hoàn chỉnh được xây dựng từ con số 0 bằng **C++17**, hỗ trợ hai chế độ hiển thị: **Cửa sổ đồ họa 2D Native Win32 GUI** (Double-buffered GDI) và **Console ANSI Terminal cổ điển**. Dự án tích hợp thuật toán sinh hầm ngục thủ tục, AI quái vật tìm đường A*, hệ thống tầm nhìn sương mù (Fog of War), cơ chế chiến đấu theo lượt (Bump combat), hệ thống túi đồ & trang bị (Inventory & Equipment), và hệ sinh thái kiểm thử tự động giả lập headless 500 ticks.

---

## 📑 Mục lục
1. [Hướng dẫn cài đặt & Biên dịch](#-hướng-dẫn-cài-đặt--biên-dịch)
2. [Các chế độ khởi chạy](#-các-chế-độ-khởi-chạy)
3. [Bảng phím điều khiển (Controls & Hotkeys)](#-bảng-phím-điều-khiển-controls--hotkeys)
4. [Cơ chế gameplay chi tiết](#-cơ-chế-gameplay-chi-tiết)
5. [Hệ thống vật phẩm & Trang bị](#-hệ-thống-vật-phẩm--trang-bị)
6. [Mẹo chiến thuật sinh tồn (Pro Tips)](#-mẹo-chiến-thuật-sinh-tồn-pro-tips)
7. [Kiến trúc mã nguồn](#-kiến-trúc-mã-nguồn)

---

## 🛠️ Hướng dẫn cài đặt & Biên dịch

### Yêu cầu hệ thống:
- Hệ điều hành: **Windows 10 / 11**
- Trình biên dịch: **GCC (MinGW-W64 >= 9.0)** hoặc **MSVC (Visual Studio 2019 trở lên)** hỗ trợ chuẩn **C++17**
- Công cụ build: **CMake >= 3.15**

### Các bước biên dịch từ mã nguồn:
Mở PowerShell hoặc Command Prompt tại thư mục dự án và thực thi:

```powershell
# 1. Cấu hình thư mục build bằng CMake (ví dụ với MinGW)
cmake -B build -G "MinGW Makefiles"

# 2. Biên dịch toàn bộ dự án (Game + Test suite)
cmake --build build
```

Sau khi build thành công, trong thư mục `build/` sẽ tạo ra 2 file thực thi:
- `build/roguelike.exe`: Game chính.
- `build/headless_test.exe`: Bộ test tự động giả lập 12 kịch bản.

---

## 🚀 Các chế độ khởi chạy

| Lệnh khởi chạy | Chế độ hiển thị | Mô tả |
| :--- | :--- | :--- |
| `.\build\roguelike.exe` | **Native Win32 2D GUI** *(Mặc định)* | Mở cửa sổ đồ họa 2D riêng biệt kích thước 1120x520 với màu sắc gạch đá 3D, thanh máu trực quan trên đầu nhân vật, và kỹ thuật Double-buffering chống giật 100%. |
| `.\build\roguelike.exe --cli` | **ANSI Console Terminal** | Chạy trực tiếp trong giao diện dòng lệnh với ký tự ASCII/ANSI cổ điển, hỗ trợ non-blocking input và double-buffering terminal. |
| `.\build\roguelike.exe --headless` | **Giả lập tự động (Headless)** | Chạy ngầm 500 ticks không cần giao diện để kiểm tra logic, bộ nhớ và độ ổn định. |
| `.\build\headless_test.exe` | **Test Suite Runner** | Chạy 12 bài kiểm thử đơn vị tự động kiểm tra map, A*, FOW, inventory, stats, và khả năng sinh tồn. |

---

## 🎮 Bảng phím điều khiển (Controls & Hotkeys)

### 1. Trong màn hình chơi chính:

| Phím bấm | Hành động | Giải thích |
| :--- | :--- | :--- |
| **`W` / `A` / `S` / `D`** hoặc **Phím Mũi Tên** | **Di chuyển / Tấn công** | Đi theo 4 hướng. Đi đâm vào quái vật sẽ tự động ra đòn (*Bump Combat*). |
| **`Space`** hoặc **`.`** | **Nghỉ / Chờ lượt (Rest)** | Bỏ qua lượt đi để nhử quái lại gần. Khi an toàn (không có quái trong tầm nhìn), cứ **5 lượt nghỉ** sẽ **hồi 1 HP**. |
| **`I`** | **Mở / Đóng Túi đồ (Inventory)** | Bật popup quản lý túi đồ và trang bị vũ khí, áo giáp, bình máu, cuộn giấy. |
| **`Shift + .`** (ký tự **`>`**) | **Xuống cầu thang (Descend)** | Khi đứng trên ô cầu thang `>` dẫn xuống tầng dưới, nhấn tổ hợp này để sang tầng tiếp theo. |
| **`R`** | **Chơi lại ngay (Quick Restart)** | Khởi tạo lại một ván game mới ngay lập tức (dùng khi chết hoặc muốn đổi map mới). |
| **`Q`** hoặc **`ESC`** | **Thoát game** | Đóng cửa sổ trò chơi. |

### 2. Khi đang mở bảng Túi đồ (`I`):

| Thao tác | Hành động | Giải thích |
| :--- | :--- | :--- |
| **Chuột Trái (Left-Click)** | **Chọn & Dùng / Trang bị** | Click chuột trực tiếp vào dòng vật phẩm trên bảng để sử dụng ngay lập tức! |
| **Chuột Phải (Right-Click)** | **Vứt bỏ (Drop)** | Click chuột phải vào món đồ để vứt ra sàn dưới chân. |
| **Phím Mũi Tên `↑` / `↓`** hoặc **`W` / `S`** | **Di chuyển con trỏ chọn** | Thanh sáng màu vàng sẽ di chuyển lên xuống giữa các món đồ. |
| **`Enter`** hoặc **`Space`** | **Dùng / Trang bị món đang chọn** | Uống bình máu, đọc cuộn giấy, hoặc mang vũ khí/giáp đang được chọn. |
| **`D`** hoặc **`Delete`** | **Vứt món đang chọn ra sàn** | Vứt món đồ đang được con trỏ trỏ tới. |
| **Phím số `1` đến `9`, `0`** | **Phím tắt nhanh** | Bấm số tương ứng (hỗ trợ cả hàng phím số trên, bàn phím Numpad và các bộ gõ tiếng Việt). |
| **`I`** hoặc **`ESC`** | **Đóng bảng túi đồ** | Quay lại điều khiển di chuyển nhân vật. |

---

## 🧠 Cơ chế gameplay chi tiết

### 1. Sinh hầm ngục thủ tục (Procedural Dungeon Generation)
- Bản đồ mỗi tầng có kích thước 80x25 ô gạch, sinh ngẫu nhiên theo thuật toán kết hợp phân tách phòng (Room Placement) và đào hầm chữ L (L-corridors).
- **Cam kết 100% liên thông**: Tất cả các phòng đều có đường đi nối với nhau, không bao giờ xuất hiện phòng cụt bị cô lập.
- Cửa ra vào (`+` đóng, `/` mở) được bố trí tự nhiên tại lối giao giữa phòng và hành lang.
- Cầu thang xuống tầng sâu hơn (`>`) luôn được đặt tại căn phòng xa nhất so với vị trí xuất phát của bạn.

### 2. Tầm nhìn & Sương mù (Bresenham Line of Sight & Fog of War)
- Sử dụng thuật toán đường thẳng **Bresenham** để tính toán góc nhìn từ tâm nhân vật với bán kính **8 ô gạch**:
  - **Vùng chưa từng đến (Unexplored)**: Tối đen hoàn toàn.
  - **Vùng đã khám phá (Explored)**: Hiển thị mờ xám giúp bạn ghi nhớ địa hình đã đi qua nhưng không thấy được quái vật đang di chuyển trong đó.
  - **Vùng trong tầm mắt (Visible)**: Chiếu sáng rõ nét với đầy đủ màu sắc, hiển thị quái vật và vật phẩm thời gian thực.

### 3. Trí tuệ nhân tạo quái vật (A* Monster AI)
- Quái vật sở hữu thuật toán tìm đường **A\* (A-Star)** với hàm ước lượng khoảng cách Manhattan.
- Khi người chơi bước vào tầm nhìn hoặc phạm vi kích động của quái, chúng sẽ tự động tính toán lộ trình ngắn nhất vượt qua mê cung, né tường và né các quái vật khác để tiếp cận và bao vây bạn.

### 4. Cơ chế Chiến đấu & Tính sát thương (Bump Combat)
- Đơn giản và trực quan: Chỉ cần di chuyển đâm thẳng vào ô của quái vật để tấn công.
- **Công thức tính sát thương**:
  $$\text{Damage} = \max(1, \text{Attack}_{\text{Player}} - \text{Defense}_{\text{Monster}})$$
- Sau khi bạn tấn công hoặc di chuyển, tất cả quái vật ở gần sẽ đồng thời thực hiện lượt hành động của chúng.

### 5. Hệ thống Cấp độ & Kinh nghiệm (Leveling & EXP)
- Tiêu diệt quái vật để nhận điểm kinh nghiệm:
  - 🟢 **Goblin**: +10 EXP
  - ⚪ **Skeleton**: +15 EXP
  - 🔴 **Orc**: +25 EXP
  - 🔥 **Dragon**: +50 EXP
- Khi thanh EXP đầy: **Level Up!**
  - **Hồi phục 100% lượng máu ngay lập tức**.
  - **Tăng vĩnh viễn**: +5 Max HP, +1 Sức tấn công (Atk), +1 Phòng thủ (Def).
  - Tăng mốc EXP yêu cầu cho cấp tiếp theo.

### 6. Mục tiêu trò chơi (Victory Condition)
- Vượt qua **3 tầng hầm ngục nguy hiểm** (`Floor 1` $\rightarrow$ `Floor 2` $\rightarrow$ `Floor 3`).
- Tìm thấy và bước xuống cầu thang ở tầng 3 để đoạt lấy bảo vật **Amulet of Yendor** và giành chiến thắng chung cuộc (**VICTORY**)!

---

## 🎒 Hệ thống vật phẩm & Trang bị

Túi đồ có sức chứa **10 ô**. Đi đè lên vật phẩm trên sàn sẽ tự động nhặt vào túi.

| Loại | Ký hiệu | Tên vật phẩm | Chỉ số / Công dụng |
| :--- | :---: | :--- | :--- |
| **Vũ khí** | `)` | **Dagger (Dao găm)** | **+3 Tấn công (Atk)** |
| | `)` | **Iron Longsword (Kiếm thép)** | **+5 Tấn công (Atk)** |
| | `)` | **Battle Axe (Rìu chiến)** | **+7 Tấn công (Atk)** |
| **Áo giáp** | `[` | **Leather Armor (Giáp da)** | **+2 Phòng thủ (Def)** |
| | `[` | **Chainmail (Giáp xích)** | **+4 Phòng thủ (Def)** |
| | `[` | **Plate Armor (Giáp sắt tấm)** | **+6 Phòng thủ (Def)** |
| **Bình máu** | `!` | **Health Potion (Bình máu)** | Hồi phục tức thì **+15 HP** (lưu trong túi đồ để uống khi cần) |
| | `!` | **Greater Potion (Bình đại hồi phục)** | Hồi phục tức thì **+25 HP** |
| **Cuộn giấy** | `?` | **Scroll of Teleportation** | **Dịch chuyển khẩn cấp**: Đưa bạn đến ngẫu nhiên một ô sàn an toàn trong hầm để thoát khỏi vòng vây của quái vật |

> **Lưu ý**: Chỉ số Atk và Def trên thanh HUD được tính động:
> - $\text{Total Atk} = \text{Base Atk (Level)} + \text{Vũ khí đang trang bị}$
> - $\text{Total Def} = \text{Base Def (Level)} + \text{Áo giáp đang trang bị}$

---

## 💡 Mẹo chiến thuật sinh tồn (Pro Tips)

1. **Tuyệt đối không đánh nhau ở phòng lớn khi bị bao vây**:
   - Nếu bạn đứng giữa phòng rộng, 2-3 quái vật có thể áp sát và cùng lúc tấn công bạn trong một lượt (nhận 5-8 sát thương dồn/lượt).
   - *Cách xử lý*: Hãy lùi dần về cửa phòng (`+` / `/`) hoặc hành lang hẹp (choke point rộng 1 ô). Khi đó chỉ có duy nhất 1 quái vật có thể đối đầu với bạn, biến thế trận thành 1v1 dễ thở hơn rất nhiều.
2. **Tận dụng cơ chế Nghỉ hồi máu (Safe Rest)**:
   - Sau khi dọn sạch một căn phòng, nếu xung quanh không còn quái vật trong tầm nhìn, hãy nhấn phím `Space` hoặc `.` liên tục. Cứ 5 lượt nghỉ bạn sẽ hồi lại 1 HP mà không cần lãng phí bình thuốc quý giá.
3. **Giữ cuộn giấy Dịch chuyển (`Scroll of Teleport`) cho tình huống ngàn cân treo sợi tóc**:
   - Khi máu tụt xuống mức báo động và bị dồn vào chân tường, hãy mở túi đồ (`I`) và đọc ngay cuộn giấy dịch chuyển để lật ngược tình thế.
4. **Trang bị vũ khí và giáp ngay khi nhặt**:
   - Đồ nhặt trên sàn sẽ nằm trong túi đồ. Bạn phải bấm `I` và nhấn số tương ứng thì đồ mới được mặc lên người (`[EQUIPPED]`).

---

## 🏛️ Kiến trúc mã nguồn

```text
roguelike-engine/
├── CMakeLists.txt              # Cấu hình biên dịch CMake (C++17, liên kết gdi32, user32)
├── README.md                   # Tài liệu hướng dẫn chơi và phát triển
├── main.cpp                    # Điểm khởi đầu chương trình (xử lý cờ --cli, --headless)
├── include/
│   ├── map.hpp                 # Lớp Map, Point, TileType, cấu trúc phòng và thuật toán sinh hầm
│   ├── ai.hpp                  # Lớp Pathfinding (A*), FogOfWar, LineOfSight (Bresenham)
│   └── engine.hpp              # Lớp Engine, Entity, Item, Inventory, Render GUI/Console
├── src/
│   ├── map.cpp                 # Cài đặt thuật toán sinh hầm ngục và phân bố spawn
│   ├── ai.cpp                  # Cài đặt A*, Raycasting và máy trạng thái AI quái vật
│   └── engine.cpp              # Cài đặt vòng lặp game, combat, Win32 GUI GDI renderer
└── tests/
    └── test_headless.cpp       # Bộ 12 bài kiểm thử tự động giả lập 500 ticks
```
