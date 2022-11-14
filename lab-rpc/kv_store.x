typedef char buf <>;

struct k_v_pair {
	buf k;
	buf v;
};

 program KVSTORE {
	version KVSTORE_V1 {
		int EXAMPLE(int) = 1;
		string ECHO(string) = 2;
		void PUT(k_v_pair) = 3;
		buf GET(buf) = 4;
	} = 1;
} = 0x20000001;
