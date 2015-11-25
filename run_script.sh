ps -ef | grep "a.out" | awk '{print $2}' | grep -v 'grep' | xargs kill -9
make
./a.out $*