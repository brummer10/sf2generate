# sf2generate

A minimal SF2 (SoundFont 2) writer for a single audio file.
Note, regardless how many channels the file have, only the first one will be used.
It could act as command-line application like the following

```shell
sf2generate input.wav output.sf2 
```
A pitch detector will calculate the RootKey and
the Pitch correction to be used in the SoundFont.

Additional the expected SampleRate could be given like

```shell
sf2generate input.wav output.sf2 48000
```
Otherwise, the SampleRate from the file will be used.

That will generate a SF2 SoundFont with two instruments, 
a SingleShoot and a LOOP.


## Features

<p align="center">
    <img src="https://github.com/brummer10/sf2generate/blob/main/sf2generate.png?raw=true" />
</p>

When starting without command-line arguments sf2generate will start with a GUI, 
allow you to select a audio file,
clip it to the range to be used for the OneShoot Instrument
and set the loop points to be used for the Looped Instrument.

<p align="center">
    <img src="https://github.com/brummer10/sf2generate/blob/main/sf2generate-settings.png?raw=true" />
</p>

The pitch detector will then calculate the RootKey and
the Pitch correction to be used in the SoundFont.
You could still change that to your needs before generate the SoundFont.

As well you could set the values in percent for Chorus and Reverb.

When done, the SoundFont could be generated and saved.

That's it, not more, not less.

The GUI is created with libxputty.


## Dependencies

- libsndfile1-dev
- libfftw3-dev
- portaudio19-dev
- libcairo2-dev
- libx11-dev

## Building from source code

```shell
git clone https://github.com/brummer10/sf2generate.git
cd sf2generate
git submodule update --init --recursive
make
sudo make install # will install into /usr/bin
```
