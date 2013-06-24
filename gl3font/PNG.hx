package gl3font;

import #if neko neko #else cpp #end.Lib;

abstract ImageData(Dynamic) from Dynamic to Dynamic {}

class PNG {
#if neko
    static var hasinit = false;
    static inline function init() {
        if (!hasinit) {
            var i = neko.Lib.load("gl3font","neko_init",5);
            if (i != null)
                i(function(s)return new String(s),function(len:Int){var r=[];if(len>0)r[len-1]=null;return r;},null,true,false);
            hasinit = true;
        }
    }
#else
    static inline function init() {}
#end

    static inline var GREY = 0;
    static inline var RGBA = 1;
    public var type:Int;

    public var width:Int;
    public var height:Int;

    public var data:ImageData;

    public function toString() {
        var type = if (type == GREY) "GREY" else "RGBA";
        return '{ $type ($width*$height) }';
    }

    function new() {}

    public static function grey(path:String):PNG { init();
        var ret = new PNG();
        ret.type = GREY;
        var dims = [0,0];
        ret.data = Lib.load("gl3font", "hx_gl3font_png_grey", 2)(path, dims);
        ret.width = dims[0];
        ret.height = dims[1];
        return ret;
    }

    public static function rgba(path:String):PNG { init();
        var ret = new PNG();
        ret.type = RGBA;
        var dims = [0,0];
        ret.data = Lib.load("gl3font", "hx_gl3font_png_rgba", 2)(path, dims);
        ret.width = dims[0];
        ret.height = dims[1];
        return ret;
    }
}
