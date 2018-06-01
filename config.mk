
# If your compiler cannot find boost, please specify it explicitly like this:
#boost_include_dir = -I/usr/local/include/
#boost_lib_dir = -L/usr/local/lib/

cflag = -Wall -fexceptions -std=c++98
ifeq (${MAKECMDGOALS}, debug)
	cflag += -g -DDEBUG
	dir = debug
else
	cflag += -O2 -DNDEBUG
	lflag = -s
	dir = release
endif
cflag += -DBOOST_ASIO_NO_DEPRECATED
common_libs = -lboost_system -lboost_thread -lboost_chrono

kernel = ${shell uname -s}
ifeq (${kernel}, SunOS)
cflag += -pthreads
lflag += -pthreads -lsocket -lnsl
else
cflag += -pthread
lflag += -pthread
ifeq (${kernel}, FreeBSD)
common_libs += -lboost_atomic
#here maybe still have other kernels need to be separated out
endif
endif

cflag += ${ext_cflag} ${boost_include_dir}
lflag += ${boost_lib_dir} ${common_libs} ${ext_libs}

target = ${dir}/${module}
sources = ${shell ls *.cpp}
objects = ${patsubst %.cpp,${dir}/%.o,${sources}}
deps = ${patsubst %.o,%.d,${objects}}
${shell mkdir -p ${dir}}

release debug : ${target}
-include ${deps}
${target} : ${objects}
	${CXX} -o $@ $^ ${lflag}
${objects} : ${dir}/%.o : %.cpp
	${CXX} ${cflag} -E -MMD -w -MT '$@' -MF ${subst .cpp,.d,${dir}/$<} $< 1>/dev/null
	${CXX} ${cflag} -c $< -o $@

.PHONY : clean
clean:
	-rm -rf debug release

