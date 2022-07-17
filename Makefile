chameleonhash:
  #for ubuntu
	#gcc -Wall -I/usr/local/include -o chameleonhash chameleonhash.c -L/usr/local/lib -lssl -lcrypto -lcurl -lm -lpthread -ldl
  #for macOS X
	gcc -Wall -I/usr/local/Cellar/openssl@3/3.0.0_1/include/ -o chameleonhash_comm chameleonhash_comm.c -L/usr/local/Cellar/openssl@3/3.0.0_1/lib -lssl -lcrypto -lcurl -lm -lpthread -ldl
clean:
	rm chameleonhash_comm
