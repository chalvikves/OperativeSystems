#!/usr/bin/env bash
# Lab 2
set -e

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd -P)

# Pintos clean code
TEMPLATE_URL="https://chalmersuniversity.box.com/shared/static/tb650k4azr3vezse0l212nbbcg7rjslg.zip" 

SUBMISSION="lab2-$(whoami)"
TEMPLATE_NAME="pintos-template"
TEST_PINTOS_DIR="pintos"
REQUIRED_PATH="$TEST_PINTOS_DIR"


echo "[NOTICE] You need to compile and test your program on the lab computers before submission!"
echo "[NOTICE] See here for more details: https://it.portal.chalmers.se/itportal/GenStud/UsingStudentComputersFromHome"
sleep 5

cd "$script_dir"
[[ ! -e "$REQUIRED_PATH" ]] && { echo "[ERROR] Script needs to be placed in the directory that contains: $REQUIRED_PATH"; exit 1; }

rm -rf "$TEMPLATE_NAME"
rm -rf "$SUBMISSION"

wget -q -O "${TEMPLATE_NAME}.zip" "$TEMPLATE_URL" > /dev/null
unzip "${TEMPLATE_NAME}.zip"

source ~/.bashrc
chmod +x "$TEMPLATE_NAME"/src/utils/pintos*
chmod +x "$TEMPLATE_NAME"/src/utils/backtrace
sleep 1

mkdir "$SUBMISSION"
cd "$SUBMISSION"

cp -r "$script_dir/$TEMPLATE_NAME"/src .
cp "$script_dir/$TEST_PINTOS_DIR"/src/threads/thread.* ./src/threads/
cp "$script_dir/$TEST_PINTOS_DIR"/src/devices/timer.* ./src/devices/

cd src/threads 
make clean > /dev/null
make
# Check if any alarm test failed
if make check | grep -i 'FAIL.*alarm-'; then
  echo "[ERROR] One or more tests failed!"
  exit 1
fi

cd "$script_dir"
zip -FSr "${SUBMISSION}.zip" "$SUBMISSION/"

echo
echo "===================================================="
echo ">> Submission prepared SUCCESSFULLY: $PWD/${SUBMISSION}.zip"
echo ">> Upload to Canvas together with your report!"
echo "===================================================="