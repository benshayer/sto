#!/bin/bash

METARUN="yes" run/run_ycsb_a.sh
METARUN="yes" run/run_ycsb_b.sh
METARUN="yes" run/run_ycsb_c.sh
sudo shutdown -h +1
