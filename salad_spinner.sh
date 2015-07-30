#!/bin/bash

while [ 1 ]; do
    echo "MARK\n" | nc -u 127.0.0.1 3456 &> /dev/null
    echo "MARK\n" | nc 127.0.0.1 4569 &> /dev/null
    sleep 1
done

