1. Client sau khi thoát chat bị in thừa dòng (lỗi có thể do chưa tắt thread nhiệm vụ ghi - chưa tìm ra giải pháp mới để khắc phục)
- Tần suất xuất hiện: lúc có lúc không (ko thể kiểm soát)
![TH_1](./BUG_IMG/bug1_img1.png)
![TH_1](./BUG_IMG/bug1_img2.png)

2. Đang chat (PVP - chat in ROOM chưa thử)
- thoát bằng cách Ctrl + C -> server core dump luôn (Chưa nghĩ ra tình huống sửa)
- Tuy nhiên thoát bằng cách gõ 'exit' vẫn thoát bình thường

BUG đã fix

1. Chat gửi nhiều bị ngắt thành 2 lần chat (Chưa nghĩ ra lý do và hướng fix)
- Do tách nhiệm vụ đọc input từ bàn phím vào thành function prompt_input_ver2 
- Nhưng hàm bị tự lặp câu lệnh printf 1 cách khó hiểu (hoặc lặp cả hàm) - khó hiểu
-> giải quyết bằng cách viết trực tiếp không gọi hàm ngoài 