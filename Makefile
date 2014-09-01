APP=dsh

all:
	gcc -g -Wall -pedantic $(APP).c -o $(APP) -lreadline 

clean:
	rm -f *.o $(APP)

install:
	cp $(APP) /bin/
	
uninstall:
	rm -f /bin/$(APP)
