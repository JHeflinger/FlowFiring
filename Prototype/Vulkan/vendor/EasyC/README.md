# EasyC: A lightweight (mostly) crossplatform C utils library

## What

EasyC is a small C library that simplifies a lot of common utils in C to make life as a C programmer easier! It includes all the essentials, such as logging, math, memory tracking, threads, data structures, and even some basic networking!

## Why

Have you ever jumped from C project to C project, noticing that theres always a `utils/` folder or a `utils.h` file that kinda just has the same stuff every time? Are you tired of rewriting the same utils every time you make a new C project? Are you tired of improving your utils and then having to go from project to project implementing these slight improvements each time? Well, me too. That's why I wrote this. Just a small library so I can keep all my utils bundled up in a nice subrepository for all my projects.

## How

This project uses my in-house C project manager tool, [tiny](https://github.com/JHeflinger/tiny). If you'd like to learn how that works, you can check out the github page or you can read about it on my blog [here](https://jasonheflinger.com/projects/tiny). If you don't care, you can basically just throw everything in the `include/` folder into your project however you wish.

## Testing

For sanity checking purposes and because I thought for once I should actually have good project practices, EasyC features a test suite to ensure that everything is working as intended. You can run these tests by using the `run.sh` script or `run.bat` script depending on your respective supported OS, or you can figure out how to compile it yourself by using the `test/main.c` file as the entrypoint.
