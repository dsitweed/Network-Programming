auth account:
signin username password
signup username password
sigout username


Tree clients node
key: when thread connect to server auto set id
value: struct Client


Tree accounts node
key: username
value: struct Account

Account *acc = (Account *) malloc(sizeof(Account));

jrb_insert_str(accounts, acc->username, new_jval_v(acc));

Account * findedAcc = (Account *) jval_v(node->val);

<!-- BUG -->
BUG:
- yêu cầu chọn menu là int nhưng nhập vào string treo luôn 
- Client thoat ra -> core dump (Ở phần chat room)
- Gửi tin dài bị ngắt
- khi exit rồi bị in thừa ra 2 dòng 
<!-- BUG -->

<!-- GHi chus -->
- delete room, delete guest se o trong man hinh chat 


Protocol:
Auth protocol:
type user_name password

Select_room_protocol:
type room_name owner_name

accounts.txt format
username password id_user

rooms.txt format
roomName ownerName id_owner num_guest
id_owner (có thể thay đổi vị trí)
id_guest1
id_getst2
...
roomName2 ownerName2 id_owner2 num_guest

(Không cần Type điều hướng đầu )
chat với PVP
destId mess

chat group 
group_name mess


Lưu trữ trong chương trình khi đang chạy:
JRB tree rooms
- Lưu các rooms
- Nếu chat PVP thì lấy thông tin recv_id từ thông tin client gửi lên server luôn 

Các hướng phát triển tiếp:
- Lưu trữ các đoạn chat của PVP - Hiển thị lên màn hình
- 
- 