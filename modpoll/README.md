https://www.modbusdriver.com/modpoll.html

<hr>

2 bytes: Voltage L1:

./modpoll -m enc -a 1 -0 -1 -r 108 -c 1 -t 3 -p 9502 10.1.0.37

<hr>

4 bytes: Total Energy Import:

./modpoll -m enc -a 1 -0 -1 -r 22 -c 1 -t 3:int -e -p 9502 10.1.0.37
