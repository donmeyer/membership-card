# README #

This README would normally document whatever steps are necessary to get your application up and running.

### What is this repository for? ###

So I built Lee Hart's slick little [1802 Membership Card](http://www.sunrise-ev.com//membershipcard.htm) and its Front Panel card. Next I wanted to load some programs onto it! There was that nice DB-25 connector all ready to connect to my PC's parallel port. However... I've got a Mac, and haven't seen a computer parallel port in perhaps a decade.

My solution to this problem was to create my own interface hardware based on an Arduino. I considered using a Spark Core to allow doing things wirelessly via WiFi, but decided that for this purpose the Arduino was something more people were likely to have, and it was just a somewhat simpler solution.

The final design uses an Arduino, a 16-line I2C port expander chip, and some software. The software consists of the Arduino program and a Python program that runs on my Mac. Since it's Python, it should also run on a Windows system, a Linux box, etc.
You can also forego the Python side of things and directly control the Arduino loader from a terminal, or from your own software if you prefer.



### How do I get set up? ###

* Download or clone this repo
* Build the hardware
* Program the Arduio

### Who do I talk to? ###

* <don@sgsw.com>