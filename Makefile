all: bankingServer bankingClient

bankingServer: bankingServer.c
	gcc bankingServer.c -o bankingServer -lpthread

bankingClient: bankingClient.c 
	gcc bankingClient.c -o bankingClient -lpthread

clean:
	rm -rf server
	rm -rf client
