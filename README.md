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


BUG:
- yêu cầu chọn menu là int nhưng nhập vào string treo luôn 
- Client thoat ra -> core dump

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
- tồn tại 1 nhánh key = "PVP" là 1 tree
    - Dạng key = id_nguoi1, value = id_nguoi2
- Các nhánh còn lại key = room_name, value = room (có kiểu Room type)