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