@ README

`sox` means [sox.sf.net](http://sox.sf.net)<BR>
`sox_ng` means this hard fork of `sox-14.4.2`<BR>
`SoX` means the Swiss Army Knife of command-line audio processing and its spirit
in any of its incarnations<BR>

The `SoX_ng` project imports, compares and refines bug fixes and new work 
from the 57 software distributions that package SoX
and from the plethora of forks on github and elsewhere,
and makes regular releases with a six-monthly cadence
for each of the micro (bug fixes) and minor (new features) releases.
Major releases (non-backwards-compatible changes) are being considered.

## How to get it

`sox_ng` lives at
[codeberg.org/sox_ng/sox_ng](https://codeberg.org/sox_ng/sox_ng)
and is composed of a SoX code base, a wiki and an issue tracker.

To fetch it:
```
git clone https://codeberg.org/sox_ng/sox_ng
cd sox_ng
```
and, optionally,
```
git clone https://codeberg.org/sox_ng/sox_ng.wiki wiki
bin/getissues	# into issues/
```

To compile it:
```
autoreconf -i
./configure
make
```
and to install it:
```
sudo make install
```

You can edit and commit to the code, which is in C, Bourne shell and autoconf,
and to the wiki, which is `.md` and image files, from Codeberg's web interface
or from the command-line.

The issues are currently read-only to the command line
and editable only on the Codeberg web site.

## Community
The SoX_ng project has two mailing lists on sourcehut.org:
`u.sox_ng.users@lists.sr.ht` and `u.sox_ng.devel@lists.sr.ht`,
both for discussion both of `sox_ng's codebase and of the project itself.

Discussion of SoX itself should remain on the sox.sf.net mailing lists.

[`SoX_ng`'s financial accounts](Accounting) are public and
[Bounties] can be offered for specific work.
