#!/bin/bash
make debug
sudo insmod clique.ko

sudo rmmod clique.ko
