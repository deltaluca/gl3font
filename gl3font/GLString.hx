package gl3font;

import haxe.Utf8;
import ogl.GLM;
import goodies.Maybe;

typedef Code = Int; //unicode code point.
typedef GLSubString = {sub:Array<Code>, col:Maybe<Vec4>};
abstract GLString(Array<GLSubString>) from Array<GLSubString> to Array<GLSubString> {
    public inline function new(xs:Array<GLSubString>) this = xs;

    static function toCodes(x:String):Array<Code> {
        var ret = [];
        Utf8.iter(x, ret.push);
        return ret;
    }
    public static inline function make(x:String, ?col:Maybe<Vec4>):GLString {
        return if (x.length == 0) [] else [{sub:toCodes(x),col:col}];
    }
    @:from public static inline function fromString(x:String):GLString {
        return if (x.length == 0) [] else [{sub:toCodes(x),col:null}];
    }

    public var length(get,never):Int;
    inline function get_length() {
        var t:Array<GLSubString> = this;
        var ret = 0;
        for (sub in t) ret += sub.sub.length;
        return ret;
    }

    public inline function empty() {
        var t:Array<GLSubString> = this;
        return t.length == 0;
    }

    public function charCodeAt(i:Int):Maybe<Code> {
        var t:Array<GLSubString> = this;
        var sub = 0;
        while (sub < t.length && i >= t[sub].sub.length) {
            i -= t[sub].sub.length;
            sub++;
        }
        return if (sub >= t.length || i >= t[sub].sub.length) null
               else t[sub].sub[i];
    }

    public function charAt(i:Int):String {
        var code = charCodeAt(i);
        if (code == null) return "";
        else {
            var ret = new Utf8();
            ret.addChar(code.extract());
            return ret.toString();
        }
    }

    public function colourAt(i:Int):Maybe<Vec4> {
        var t:Array<GLSubString> = this;
        var sub = 0;
        while (sub < t.length && i >= t[sub].sub.length) {
            i -= t[sub].sub.length;
            sub++;
        }
        return if (sub >= t.length || i >= t[sub].sub.length) null
               else t[sub].col;
    }

    public function normalise(col:Maybe<Vec4>):GLString {
        var t:Array<GLSubString> = this;
        if (t.length == 0) return this;
        var ret:Array<GLSubString> = [{sub:[],col:col}];
        for (sub in t) ret[0].sub = ret[0].sub.concat(sub.sub);
        return ret;
    }

    public function substr(ind:Int, count:Int=-1):GLString {
        if (count == 0 || empty()) return [];
        var t:Array<GLSubString> = this;
        var ret:Array<GLSubString> = [];
        for (sub in t) {
            var part = sub.sub.slice(ind, count < 0 ? sub.sub.length : ind+count);
            if (part.length != 0) ret.push({sub:part, col:sub.col});
            ind -= sub.sub.length;
        }
        return ret;
    }

    // ignores colour.
    public function indexOf(str:GLString, ?startIndex:Maybe<Int>):Int {
        var ind = if (startIndex == null) 0 else startIndex.extract();
        var slen = str.length;
        var len = length - slen;
        while (ind <= len) {
            var match = true;
            for (i in 0...slen) {
                if (charCodeAt(i+ind) != str.charCodeAt(i)) {
                    match = false;
                    break;
                }
            }
            if (match) return ind;
            else ind++;
        }
        return -1;
    }

    // ignores colour.
    public function lastIndexOf(str:GLString, ?startIndex:Maybe<Int>):Int {
        var slen = str.length;
        var len = length - slen;
        var ind = if (startIndex == null) len else Std.int(Math.min(len, startIndex.extract()));
        while (ind >= 0) {
            var match = true;
            for (i in 0...slen) {
                if (charCodeAt(i+ind) != str.charCodeAt(i)) {
                    match = false;
                    break;
                }
            }
            if (match) return ind;
            else ind--;
        }
        return -1;
    }

    public function split(on:Int):Array<GLString> {
        var t:Array<GLSubString> = this;
        if (t.length == 0) return [];
        var ret:Array<Array<GLSubString>> = [];
        var cur = [];
        for (sub in t) {
            var cursub = [];
            for (code in sub.sub) {
                if (code == on) {
                    if (cursub.length != 0) cur.push({sub:cursub, col:sub.col});
                    cursub = [];
                    if (cur.length != 0) ret.push(cur);
                    cur = [];
                }
                else cursub.push(code);
            }
            if (cursub.length != 0) cur.push({sub:cursub, col:sub.col});
            cursub = [];
        }
        if (cur.length != 0) ret.push(cur);
        return ret;
    }

    // Considering colours of substrings in a,b
    //    (xs,x) ++ (ys,null) = (xs++ys,x)
    //    (xs,null) ++ (ys,y) = (xs++ys,y)
    // so null colours are subsumed on either side (left with preference)
    static inline function joinColours(a:GLSubString, b:GLSubString):Maybe<GLSubString> {
        // Haxe issue: #11 github (can't compare a.col, b.col directly due to overload on Vec4)
        return if ((a.col != null && b.col != null && a.col.extract() == b.col.extract())
                || a.col == null
                || b.col == null)
            { sub:a.sub.concat(b.sub), col:if (a.col==null) b.col else a.col };
        else null;
    }

/* // not used.
    public inline function optimise():GLString {
        var t:Array<GLSubString> = this;
        if (t.length == 0) return [];
        var ret:Array<GLSubString> = [];
        var pre = t[0];
        for (i in 1...t.length) {
            var join = join(pre, t[i]);
            if (join != null) pre = join.extract();
            else {
                ret.push(pre);
                pre = t[i];
            }
        }
        ret.push(pre);
        return ret;
    }
*/
    public static function join(xs:Array<GLString>, ?with:Maybe<GLString>):GLString {
        if (xs.length == 0) return [];
        if (xs.length == 1) return xs[0];
        var ret:GLString = xs[0];
        for (i in 1...xs.length) {
            if (with != null) ret += with.extract();
            ret += xs[i];
        }
        return ret;
    }

    @:op(A+B) public static function concat(a:GLString, b:GLString):GLString {
        var at:Array<GLSubString> = a;
        var bt:Array<GLSubString> = b;
        var join = if (at.length != 0 && bt.length != 0) joinColours(at[at.length-1], bt[0]) else null;
        if (join == null) return at.concat(bt);
        else return at.slice(0,at.length-1).concat([join.extract()]).concat(bt.slice(1));
    }

    @:op(A+B) public static function concatS(a:GLString, b:String):GLString
        return concat(a, b);
    @:op(A+B) public static function concatS_(a:String, b:GLString):GLString
        return concat(a, b);

    // ignores colour
    @:op(A==B) public static function eq(a:GLString, b:GLString):Bool {
        return a != null && b != null && a.toString() == b.toString() || a == null && b == null;
    }

    public inline function iter(f:Code->Maybe<Vec4>->Void) {
        var t:Array<GLSubString> = this;
        for (sub in t)
        for (code in sub.sub)
            f(code, sub.col);
    }

    public function toString():String {
        var t:Array<GLSubString> = this;
        var ret = new Utf8();
        for (sub in t) for (char in sub.sub) ret.addChar(char);
        return ret.toString();
    }
}
