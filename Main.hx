package;

import gl3font.GLString;

class Main {
    static function main() {

        var x:GLString = "Hello world!";
        x = x.normalise([1,2,3,4]);
        trace(x);
        trace(untyped x);

        var y:GLString = "Hi there!";
        y = y.normalise([3,4,5,6]);
        trace(y);
        trace(untyped y);

        trace(x+y);
        trace(untyped (x+y));

        y = y.normalise([1,2,3,4]);
        trace(x+y);
        trace(untyped (x+y));

        y = y.normalise(null);
        trace(x+y);
        trace(untyped (x+y));

        x = x.normalise(null);
        trace(x+y);
        trace(untyped (x+y));

        x = x.normalise([1,2,3,4]);
        trace(x+y);
        trace(untyped (x+y));

        y = y.normalise([4,5,6,7]);
        trace((x+y).split('o'.code));

        trace("\n\n");

        var xs:Array<GLString> = [x.normalise(null), y.normalise(null)];
        trace(GLString.join(xs));
        trace(GLString.join(xs, "~~"));
    }
}
