typedef struct con{
	unsigned short  clean_session:1;
	unsigned char * client_id;
	unsigned short  client_id_len;
	unsigned short  keep_alive_time;
}connection_info;

int process_connect(unsigned char *, connection_info *);