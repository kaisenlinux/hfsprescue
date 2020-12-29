
# CC = g++ -Wuninitialized 
#-Wunused -Wunused-result -D_FILE_OFFSET_BITS=64


bin_PROGRAMS = hfsprescue

hfsprescue_SOURCES = hfsprescue.cpp hfsprescue.h \
		     bytescan.cpp bytescan.h \
		     help.cpp help.h \
		     tools.cpp tools.h \
		     decompress.cpp decompress.h \
		     eof.cpp eof.h \
		     vh.cpp vh.h \
		     utf8mac.cpp utf8mac.h \
		     log.cpp log.h \
		     ioctl.cpp ioctl.h \
		     rmdir.cpp rmdir.h \
		     apple.h freebsd.h

hfsprescue_LDADD = $(AM_LDFLAGS) $(LIBZ_LDFLAGS) 

#-L/usr/lib -L/lib -lz

