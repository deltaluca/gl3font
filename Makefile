haxelib:
	rm -f gl3font.zip
	zip -r gl3font gl3font free dejavu samples haxelib.json -x \*samples/bin\*
	haxelib local gl3font.zip
