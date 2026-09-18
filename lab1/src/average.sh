#!/bin/bash

if [ $# -eq 0 ]; then
    echo "Использование: $0 число1 число2 ..."
    exit 1
fi

echo "Количество аргументов: $#"
echo "$@" | tr ' ' '\n' | awk '{sum+=$1; n++} END {printf "Среднее арифметическое: %.2f\n", sum/n}'
