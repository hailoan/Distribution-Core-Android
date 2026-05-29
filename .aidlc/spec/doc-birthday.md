# Happy Birthday

## Tin nhắn chúc mừng

### Điều kiện
Hệ thống chạy cron job đến ngày sinh nhật của user → Pop-up greeting card + Gửi tin nhắn chúc mừng

---

### Greeting card hiển thị lần đầu

| Function/Component | Description |
|--------------------|-------------|
| Điều kiện | Hệ thống chạy cron job đến ngày sinh nhật của user → Pop-up greeting card + Gửi tin nhắn chúc mừng |
| Greeting card hiển thị lần đầu | - **Template:** Sử dụng template giống GC ngày 13/9 <br> - **Toggle** Cho phép nhận post chúc mừng và chia sẻ ngày sinh nhật cùng đồng nghiệp trên FPT Place / FPT Chat: <br>&nbsp;&nbsp;- Chỉ hỏi vào lần đầu tiên <br>&nbsp;&nbsp;- **ON (Default):** Hiển thị toggle Nhận thông báo sinh nhật tại profile của user <br>&nbsp;&nbsp;- **OFF:** Ẩn toggle Nhận thông báo sinh nhật tại profile của user |
| Greeting card hiển thị từ lần thứ 2 trở đi | Hủy button: <br> - **Trigger:** Tại MH card view → Click vào MH <br> - **Download icon:** <br>&nbsp;&nbsp;- Lưu hình ảnh về library <br>&nbsp;&nbsp;- Pop-up toast msg: Tải ảnh về thành công <br> - **Forward icon:** <br>&nbsp;&nbsp;- Chạy kịch bản forward tin nhắn <br>&nbsp;&nbsp;- Dạng file: PNG/JPG <br> - **Share icon:** <br>&nbsp;&nbsp;- Hiện thị bottom sheet share mặc định của OS <br>&nbsp;&nbsp;- Dạng file: PNG/JPG <br> - **Close icon:** Click đóng card view → Back về chat box |

---

### Tin nhắn chúc mừng

| Function/Component | Description |
|--------------------|-------------|
| Tin nhắn chúc mừng | - **Vị trí hiển thị:** Chatbox với bot <br> - **Button click nhận thiệp:** Bấm → Pop-up Greeting card hiển thị từ lần thứ 2 trở đi |

---

## Profile Setting

### Hồ sơ thông tin

Có hai trạng thái:
- **Hồ sơ thông tin (Không có DS sinh nhật)**
- **Hồ sơ thông tin (Có DS sinh nhật)**
- **Config hiển thị ngày sinh**

| Function/Component | Description |
|--------------------|-------------|
| Thông tin cá nhân | - Bổ sung hiển thị ngày sinh <br> - Quản lý danh sách nhận thông báo về sinh nhật của các user khác |

---

### PU Config Ngày sinh

**Trigger:** Bấm ngày sinh → Mở PU

- **Header:** Ngày sinh
- **Description:** "Chia sẻ thông tin ngày sinh. Tất cả người dùng đều có thể nhìn thấy thông tin này."
- **Checkbox:** Lựa chọn option hiển thị ngày sinh:
  - **Option 1:** Hiển thị ngày tháng sinh — Chỉ hiển thị ngày tháng sinh tại profile user
  - **Option 2:** Hiển thị năm sinh — Chỉ hiển thị năm sinh tại profile user
  - **Option 1+2:** Hiển thị cả ngày tháng + năm sinh tại profile user
  - **Option 3:** Cho phép hệ thống tự động bài chúc mừng sinh nhật trên FPT Place — Trigger sang FPT Place đăng bài trên trang cá nhân của user khi đến ngày sinh nhật
  - **Không chọn option nào:**
    - Không hiển thị ngày tháng năm sinh
    - Không đăng bài chúc mừng sinh nhật trên FPT Place

**Tại MH GC hiển thị lần đầu, nếu toggle:**
- **ON:**
  - Checked ngày tháng sinh
  - Checked Cho phép hệ thống tự động bài...
- **OFF:**
  - Unchecked all option

**Btn Lưu:**
- Lần đầu: Có thể bấm Lưu khi ko checked vào checkbox
- Lần thứ 2: Enable khi có thay đổi

---

### Danh sách nhận thông báo về sinh nhật

- **Header:** Thông báo sinh nhật
- **Description:** "Danh sách nhận thông báo sinh nhật đồng nghiệp mà đang bạn theo dõi"
- Danh sách bao gồm các user mà LA đã turn ON toggle Nhận thông báo sinh nhật hoặc đã được thêm vào Danh sách nhận thông báo về sinh nhật tại Profile setting
- **User info:**
  - Avatar
  - Tên user
  - Account
  - Đơn vị
  - Sinh nhật của user: Hiển thị theo config của từng user
- **Icon trash:** Bấm xóa user khỏi danh sách (User sẽ hiển thị trở lại khi user search user cần nhận thông báo)
- **Btn +:** Bấm mở giao diện thêm user muốn nhận thông báo về sinh nhật

---

### Thêm user nhận thông báo về sinh nhật

- **Search bar:** Search all user không bao gồm user đã bị deactivated
- **Danh sách user gợi ý:** Bao gồm các user cùng đơn vị với LA và xếp theo thứ tự liên hệ thường xuyên nhất đến ít liên hệ nhất
- **User info:** Hiển thị giống Danh sách nhận thông báo về sinh nhật
- **Btn +:**
  - User được thêm vào danh sách nhận thông báo về sinh nhật
  - Remove user khỏi danh sách tìm kiếm khi thêm user nhận thông báo

---

## Chat Profile

### Toggle Nhận thông báo sinh nhật

| Function/Component | Description |
|--------------------|-------------|
| Toggle Nhận thông báo sinh nhật | - **Vị trí:** <br>&nbsp;&nbsp;- Tại MH Profile full <br>&nbsp;&nbsp;- Tại MH Profile card <br> - **Điều kiện:** Hiển thị toggle khi user đồng ý share ngày sinh nhật tại PU config ngày sinh <br> - **Trạng thái toggle:** <br>&nbsp;&nbsp;- **ON:** Thêm user vào danh sách nhận thông báo <br>&nbsp;&nbsp;- **OFF (Default):** <br>&nbsp;&nbsp;&nbsp;&nbsp;- User ko có trong danh sách: Không có action gì xảy ra <br>&nbsp;&nbsp;&nbsp;&nbsp;- User đang nằm trong danh sách: Loại bỏ user khỏi danh sách <br> - Đến ngày sinh nhật của user → Gửi tin nhắn thông báo về sinh nhật |

---

## Thông báo sinh nhật

### Tin nhắn thông báo gửi từ bot

| Function/Component | Description |
|--------------------|-------------|
| Tin nhắn thông báo gửi từ Bot | - **Thời gian gửi:** 7h sáng vào ngày sinh nhật của user mà LA đăng ký nhận thông báo <br> - **Nội dung:** FCC cung cấp <br> - **Btn Gửi lời chúc:** Bấm mở MH cấu hình lời chúc |
| Cấu hình lời chúc | - **Section người nhận:** <br>&nbsp;&nbsp;- Avatar <br>&nbsp;&nbsp;- Tên <br>&nbsp;&nbsp;- Account <br>&nbsp;&nbsp;- Đơn vị <br> - **Section chọn thiệp:** <br>&nbsp;&nbsp;- Thiệp: <br>&nbsp;&nbsp;&nbsp;&nbsp;- Ảnh thiệp: Des cung cấp <br>&nbsp;&nbsp;&nbsp;&nbsp;- Chỉ chọn trong list được cung cấp <br>&nbsp;&nbsp;&nbsp;&nbsp;- Không có action thêm, sửa, xóa <br>&nbsp;&nbsp;- Lời nhắn: <br>&nbsp;&nbsp;&nbsp;&nbsp;- Max = 5000 ký tự <br>&nbsp;&nbsp;&nbsp;&nbsp;- Tin nhắn dạng text + emoji <br>&nbsp;&nbsp;&nbsp;&nbsp;- Không hỗ trợ markdown <br> - **Btn Gửi:** Bấm gửi thiệp cho user |

---

### Lời chúc trong chatbox

- **Loại tin nhắn:** Hình ảnh kèm caption
