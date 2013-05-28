package;

import gl3font.Font;
import gl3font.GLString;

class Main {
    static function main() {
        var x:GLString = "AB";
        trace(x.substr(1));
        return;
        trace(StringBuffer.getLines("hello"));
        trace("\n\n");
        trace(StringBuffer.getLines("hello\n"));
        trace("\n\n");
        trace(StringBuffer.getLines("hello\n\n"));
        trace("\n\n");
        trace(StringBuffer.getLines("hello\n\n\n"));
        return;

        var x = GLString.make("abcd", [1,1,1,1]);
        trace(untyped x);
        trace(untyped x.substr(0,4));
        trace(untyped x.substr(4));
        var y = GLString.concat(x.substr(0,4), "\n");
        trace(untyped y);
        $type(x.substr(0,4));
        $type("\n");
        $type(x.substr(0,4) + "\n");
        y = x.substr(0,4) + "\n";
        trace(untyped y);


        return;

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

        trace("\n\n");

        trace(x.indexOf('o'));
        trace(x.lastIndexOf('o'));
    }
}
