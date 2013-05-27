haxelib:
	rm -f gl3font.zip
	zip -r gl3font gl3font free dejavu samples haxelib.json -x \*samples/bin\*
	haxelib local gl3font.zip
	haxe -main Main -lib gl3font -cpp bin -D HXCPP_M64
	./bin/Main
