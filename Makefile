haxelib:
	rm -rf obj bin all_objs ndll
	make lib
	rm -f gl3font.zip
	zip -r gl3font gl3font free dejavu samples haxelib.json ndll -x \*samples/bin\*
	haxelib local gl3font.zip
	haxe -x Test.hx -lib gl3font

lib:
	haxelib run hxcpp Build.xml -DHXCPP_M64
