# PboExplorer

PboExplorer is a Windows explorer integration to provide PBO support. It works like zip files but for PBO's!


# Installation

1. Download the latest [Release](https://github.com/dedmen/PboExplorer/releases). Use the AIO zip file unless you know how to install manually.
2. Create folder `%localappdata%\Arma 3\PboExplorer\` and unpack all files into it.
3. Run "register.cmd" as Administrator. This will register PboExplorer with the windows explorer.

Installing will overwrite already configured "open with" options for .pbo files.
There should be no need to restart explorer.

To uninstall PboExplorer, open the same folder and run unregister.cmd. You will then most likely have to restart explorer before you can delete the remaining files.
Uninstalling will not restore previous pbo file associations.

# Features

## Exploring PBO's

You can open PBO files by simply double-clicking them and then browse files as if it were just a zip folder.

https://github.com/dedmen/PboExplorer/assets/3768165/d4c1e069-dc23-4d0e-b052-1333d27621af

It also includes previewing .paa texture files

https://github.com/user-attachments/assets/bf1495de-8f2d-44df-b617-9884a1672f18

## Integration with Various tools

PboExplorer tries to be a one-stop-shop and integrates with common modding tools like Arma 3 Tools and Mikero's Tools.

For UI based tools it even tries to pre-fill the UI values, like in Addon Builder or Mikero's PboProject


https://github.com/dedmen/PboExplorer/assets/3768165/23292cd1-c942-4e49-8170-807a6f904e05

## Viewing PBO properties

(Still WIP Feature)
The file properties have a Tab for PboExplorer, which currently only shows the PBO Properties as a (editable!) list.

![explorer_2023-08-28_17-41-00](https://github.com/dedmen/PboExplorer/assets/3768165/e2f2fc3e-3308-4271-b570-793a60217581)

https://github.com/dedmen/PboExplorer/assets/3768165/52728087-50ba-4231-a832-910f6c30e2e7

## Quick Editing

PboExplorer not only allows you to view PBO's, it also allows you to edit them in-place.
One example is editing the PBO Properties above. But you can quickly open a PBO and Add/Remove/Modify files inside it, without fully unpacking/repacking it.
PBOExplorer will patch the PBO with minimal changes, so it even is very fast on large PBO files.

(This feature is NOT compatible with bisigning. Signing PBO's will require a full repack or defragmentation (not implemented yet), otherwise the dummy/temporary files created inside the PBO will mess up signatures)

https://github.com/dedmen/PboExplorer/assets/3768165/3f4a132a-4a56-407f-b016-d10bf5d23130


# Known Issues/Todo's

- Updates are forced, you cannot refuse a remote update #TODO https://github.com/dedmen/PboExplorer/blob/master/Updater.cpp#L324
- Updates are not digitally signed, this is a security risk (If someone redirects your DNS or hacks my server) and is a #TODO


# Support

If you need help/information or want to contribute, feel free to join my Discord Server: https://discord.gg/vbFje5B


You can support me on patreon!
<a href="https://www.patreon.com/join/dedmen">
    <img src="https://c5.patreon.com/external/logo/become_a_patron_button.png" alt="Become a Patron!">
</a>
