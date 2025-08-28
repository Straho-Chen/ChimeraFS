#!/usr/bin/env bash


make -j4

echo "Running tests without order elimination..."
sudo ./main 0

echo "Running tests with order elimination..."
sudo ./main 1