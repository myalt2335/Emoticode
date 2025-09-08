package main

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
)

const usage = `
Emoticode CLI (v 2.A005.14.0808.K9GL@df60e) - Run and build .O_O files

Usage:
  emo build <file.O_O> [-k/--keep]   Compile code with the compiler (use -k/--keep to keep the transpiled c)
  emo run <file.O_O>                 Run code with the interpreter
  emo run --version                  Show interpreter version
  emo build --version                Show compiler version
  emo help                           Show this message
  
`

func main() {
	if len(os.Args) < 2 {
		fmt.Print(usage)
		os.Exit(1)
	}

	command := os.Args[1]
	args := os.Args[2:]

	if command == "help" || command == "--help" || command == "-h" {
		fmt.Print(usage)
		return
	}

	localAppData := os.Getenv("LocalAppData")
	if localAppData == "" {
		fmt.Println("Error: LOCALAPPDATA not set (are you on Windows?)")
		os.Exit(1)
	}

	binDir := filepath.Join(localAppData, "Emoticode", "bin")

	var binary string
	switch command {
	case "run":
		binary = filepath.Join(binDir, "emotinterpret.exe")
	case "build":
		binary = filepath.Join(binDir, "emoticompile.exe")
	default:
		fmt.Printf("Unknown command: %s\n", command)
		fmt.Print(usage)
		os.Exit(1)
	}

	if _, err := os.Stat(binary); os.IsNotExist(err) {
		fmt.Printf("Error: missing binary at %s\n", binary)
		os.Exit(1)
	}

	cmd := exec.Command(binary, args...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	cmd.Stdin = os.Stdin

	if err := cmd.Run(); err != nil {
		fmt.Fprintf(os.Stderr, "Error running %s: %v\n", binary, err)
		os.Exit(1)
	}
}
