#!/bin/bash
source ~/.rpi-sync-config
rsync -azh --stats --progress --no-p --no-g --chmod=ugo=rwX --include-from=.vscode/sync-include.txt --exclude-from=.vscode/sync-exclude.txt ./ $RPI_USER@$RPI_HOST:$RPI_PATH/