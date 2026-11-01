# Introduction
## TEMPLATE
A file named `TEMPLATE` should be in your project directory, this file
specifies basic data for _cTemple_ to compile your project.

### TEMPLATE contents
The template shall have:
- **NAME**: The name of your final binary. Placed inside _BUILDDIR_.
- **SRCDIRS**: Space-separated list of the directories where your source files are.
- **BUILDDIR**: The directory to place the binary of your project. In a _BUILDDIR_/*cur_label*/ format.
- **CC**: The name of the compiler.
- **EXT**: The extension of your source files.

Aditionally, the template may contain:
- **ARGS**: Optional arguments passed to *CC*.
- **FLAGFILE**: A file contaning arguments provided to _CC_. One argument per line.
- **SUBDIR**: A directory that _cTemple_ will chdir into and execute itself. May provide arguments passed to _cTemple_ after the dir.
- **PRE_CC**: Passed to _/bin/sh_ and executed before any compilation is done.
- **PST_CC**: Same as *PRE_CC*, but done after the build process
- **RUN_ARGS**: Arguments used with the _-r_ option.
- **RUN_ENV**: Environments variable used with the _r_ option. In a _name=value_ format.

All paths are relative to the template.

## Labels
You can specify custom "presets" (such as debug/release) in the following format: `<labelname>:`.\
If *cur_label* was specified, _cTemple_ will search for *cur_label* inside `TEMPLATE`, and parse until in encounters another label.\
If *cur_label* is not present in `TEMPLATE`, _cTemple_ will end with a `NOMATCH` error.\
If *cur_label* was not specified, _cTemple_ uses the first label (that is not the _global label_).\
Using a diferent label than the one used for the last run of _cTemple_ will trigger a recompilation of all source files.

### 'global' label
_cTemple_ searchs for a _global label_ (named simply `global`) before *cur_label*. This allows —for example— to specify _SRCDIRS_ only once.\
All keys specified in the _global label_ can be overwritten by other labels.

## Options
_cTemple_ accepts only 3 options:
- **-jN**: Number of max parallel jobs to use, where `N` is the number.
- **-f**: Dont check if file changed. Compile everything.
- **-r**: Run the resulting binary after build process. See _RUN*_ templates for arguments and environment variables passed.
- **-q**: Dont echo the compiler nor flags used.
- **-d**: Delete all object files and compile again. Used if a source files was
  deleted.
- **-l** ***label***: Specify the *cur_label*.

# TODO
Most likely almost everything. Not even sure if multiple srcdirs work.

## BUGS
Lots of them.\
SEGV.
