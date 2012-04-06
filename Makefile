dcpu16: dcpu16VM.c
	cc -o $@ -std=c99 $<

clean:
	rm -f dcpu16
