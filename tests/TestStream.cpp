#include "Stream.h"

using namespace std;
using namespace protocol;

const int MaxItems = 16;

struct TestObject : public Object
{
    int a,b,c;
    uint32_t d : 8;
    uint32_t e : 8;
    uint32_t f : 8;
    bool g;
    int numItems;
    int items[MaxItems];

    TestObject()
    {
        a = 0;
        b = 0;
        c = 0;
        d = 0;
        e = 0;
        f = 0;
        g = false;
        numItems = 0;
        memset( items, 0, sizeof(items) );
    }

    void Init()
    {
        a = 1;
        b = -2;
        c = 150;
        d = 55;
        e = 255;
        f = 127;
        g = true;
        numItems = MaxItems / 2;
        for ( int i = 0; i < numItems; ++i )
            items[i] = i + 10;        
    }

    void Serialize( Stream & stream )
    {
        serialize_int( stream, a, 0, 10 );
        serialize_int( stream, b, -5, +5 );
        serialize_int( stream, c, -100, 10000 );

        serialize_bits( stream, d, 6 );
        serialize_bits( stream, e, 8 );
        serialize_bits( stream, f, 7 );

        serialize_bool( stream, g );

        serialize_int( stream, numItems, 0, MaxItems - 1 );
        for ( int i = 0; i < numItems; ++i )
            serialize_bits( stream, items[i], 8 );
    }
};

void test_stream()
{
    cout << "test_stream" << endl;

    const int BufferSize = 256;

    uint8_t buffer[BufferSize];

    // write the object

    TestObject writeObject;
    writeObject.Init();
    {
        Stream stream( STREAM_Write, buffer, BufferSize );
        writeObject.Serialize( stream );
        stream.Flush();
    }

    // read the object back

    TestObject readObject;
    {
        Stream stream( STREAM_Read, buffer, BufferSize );
        readObject.Serialize( stream );
    }

    // verify read object matches written object

    assert( readObject.a == writeObject.a );
    assert( readObject.b == writeObject.b );
    assert( readObject.c == writeObject.c );
    assert( readObject.d == writeObject.d );
    assert( readObject.e == writeObject.e );
    assert( readObject.f == writeObject.f );
    assert( readObject.g == writeObject.g );
    assert( readObject.numItems == writeObject.numItems );
    for ( int i = 0; i < readObject.numItems; ++i )
        assert( readObject.items[i] == writeObject.items[i] );
}

int main()
{
    try
    {
        test_stream();
    }
    catch ( runtime_error & e )
    {
        cerr << string( "error: " ) + e.what() << endl;
    }

    return 0;
}