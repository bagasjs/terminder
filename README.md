# TERMINDER
A terminal based reminder program 

## USAGE
- Recommendation
It's recommended to run the terminder in your **.bashrc**, **.zshrc**, etc. Just like how you do with programs like neofetch
- Adding TODOs
```sh 
terminder add -f todo.txt 
```
```sh 
terminder add "Something" "2024-10-10 10:10:10"
```
```sh 
// This will open default editor
terminder add 
```

## INSTALATION
Building could be done via Makefile or you can just compile with the following command
```sh
clang ./src/sqlite3/shell.c ./src/sqlite3/sqlite3.c ./src/terminder.c
```
