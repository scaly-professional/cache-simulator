LAB = cachelab
COURSECODE = 15213-f20
SAN_LIBRARY_PATH = /afs/cs.cmu.edu/academic/class/15213/lib/
ifneq (,$(wildcard ../autograder-lib))
  SAN_LIBRARY_PATH = ../autograder-lib
endif
