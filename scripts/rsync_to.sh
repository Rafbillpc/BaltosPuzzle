#!/bin/sh
rsync --progress -auvh --exclude=.git --exclude=.cache --exclude=build/ . $1:~/Baltos
