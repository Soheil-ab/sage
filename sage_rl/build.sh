g++ -pthread src/mem-manager.cc src/flow.cc -o mem-manager
g++ -pthread src/sage.cc src/flow.cc -o sage
g++ src/client.c -o client
mv mem-manager rl_module/
mv client sage rl_module/

mkdir -p log
mkdir -p dataset
