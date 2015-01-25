#include "CompressionDemo.h"

#ifdef CLIENT

#include "Cubes.h"
#include "Global.h"
#include "Snapshot.h"
#include "Font.h"
#include "FontManager.h"
#include "protocol/Stream.h"
#include "protocol/SlidingWindow.h"
#include "protocol/SequenceBuffer.h"
#include "protocol/PacketFactory.h"
#include "network/Simulator.h"

static const int LeftPort = 1000;
static const int RightPort = 1001;
static const int MaxSnapshots = 256;

enum Context
{
    CONTEXT_SNAPSHOT_SLIDING_WINDOW,                // send snapshots (for serialize write)
    CONTEXT_SNAPSHOT_SEQUENCE_BUFFER,               // recv snapshots (for serialize read)
    CONTEXT_QUANTIZED_SNAPSHOT_SLIDING_WINDOW,      // quantized send snapshots (for serialize write)
    CONTEXT_QUANTIZED_SNAPSHOT_SEQUENCE_BUFFER,     // quantized recv snapshots (for serialize read)
    CONTEXT_QUANTIZED_INITIAL_SNAPSHOT              // quantized initial snapshot
};

enum SnapshotMode
{
    COMPRESSION_MODE_UNCOMPRESSED,
    COMPRESSION_MODE_ORIENTATION,
    COMPRESSION_MODE_AT_REST,
    COMPRESSION_MODE_QUANTIZE_POSITION,
    COMPRESSION_MODE_DELTA_NOT_CHANGED,
    COMPRESSION_MODE_DELTA_RELATIVE_POSITION,
    COMPRESSION_MODE_DELTA_RELATIVE_ORIENTATION,
    COMPRESSION_MODE_DELTA_CHANGED_INDICES,
    COMPRESSION_NUM_MODES
};

const char * compression_mode_descriptions[]
{
    "Uncompressed",
    "Orientation",
    "At rest",
    "Quantize position",
    "Delta not changed",
    "Delta relative position",
    "Delta relative orientation",
    "Delta changed indices"
};

struct CompressionModeData : public SnapshotModeData
{
    CompressionModeData()
    {
        playout_delay = 0.1f;        // one lost packet = no problem. two lost packets in a row = hitch
        send_rate = 60.0f;
        latency = 0.0f;
        packet_loss = 5.0f;
        jitter = 2 / 60.0f;
        interpolation = SNAPSHOT_INTERPOLATION_LINEAR;
    }
};

static CompressionModeData compression_mode_data[COMPRESSION_NUM_MODES];

static void InitCompressionModes()
{
    compression_mode_data[COMPRESSION_MODE_UNCOMPRESSED].interpolation = SNAPSHOT_INTERPOLATION_HERMITE;
    compression_mode_data[COMPRESSION_MODE_ORIENTATION].interpolation = SNAPSHOT_INTERPOLATION_HERMITE;
    compression_mode_data[COMPRESSION_MODE_AT_REST].interpolation = SNAPSHOT_INTERPOLATION_HERMITE;
}

typedef protocol::SlidingWindow<Snapshot> SnapshotSlidingWindow;
typedef protocol::SequenceBuffer<Snapshot> SnapshotSequenceBuffer;

typedef protocol::SlidingWindow<QuantizedSnapshot> QuantizedSnapshotSlidingWindow;
typedef protocol::SequenceBuffer<QuantizedSnapshot> QuantizedSnapshotSequenceBuffer;

enum CompressionPackets
{
    COMPRESSION_SNAPSHOT_PACKET,
    COMPRESSION_ACK_PACKET,
    COMPRESSION_NUM_PACKETS
};

struct CompressionSnapshotPacket : public protocol::Packet
{
    uint16_t sequence;
    uint16_t base_sequence;
    bool initial;
    int compression_mode;

    CompressionSnapshotPacket() : Packet( COMPRESSION_SNAPSHOT_PACKET )
    {
        sequence = 0;
        compression_mode = COMPRESSION_MODE_UNCOMPRESSED;
    }

    PROTOCOL_SERIALIZE_OBJECT( stream )
    {
        const int QuantizedPositionBoundXY = UnitsPerMeter * PositionBoundXY;
        const int QuantizedPositionBoundZ = UnitsPerMeter * PositionBoundZ;

        const vectorial::vec3f position_min( -PositionBoundXY, -PositionBoundXY, 0 );
        const vectorial::vec3f position_max( +PositionBoundXY, +PositionBoundXY, PositionBoundZ );

        auto snapshot_sliding_window = (SnapshotSlidingWindow*) stream.GetContext( CONTEXT_SNAPSHOT_SLIDING_WINDOW );
        auto snapshot_sequence_buffer = (SnapshotSequenceBuffer*) stream.GetContext( CONTEXT_SNAPSHOT_SEQUENCE_BUFFER );
        auto quantized_snapshot_sliding_window = (QuantizedSnapshotSlidingWindow*) stream.GetContext( CONTEXT_QUANTIZED_SNAPSHOT_SLIDING_WINDOW );
        auto quantized_snapshot_sequence_buffer = (QuantizedSnapshotSequenceBuffer*) stream.GetContext( CONTEXT_QUANTIZED_SNAPSHOT_SEQUENCE_BUFFER );
        auto quantized_initial_snapshot = (QuantizedSnapshot*) stream.GetContext( CONTEXT_QUANTIZED_INITIAL_SNAPSHOT );

        serialize_uint16( stream, sequence );

        serialize_int( stream, compression_mode, 0, COMPRESSION_NUM_MODES - 1 );

        serialize_bool( stream, initial );

        if ( !initial )
            serialize_uint16( stream, base_sequence );

        CubeState * cubes = nullptr;
        QuantizedCubeState * quantized_cubes = nullptr;

        if ( compression_mode < COMPRESSION_MODE_QUANTIZE_POSITION )
        {
            if ( Stream::IsWriting )
            {
                CORE_ASSERT( snapshot_sliding_window );
                auto & entry = snapshot_sliding_window->Get( sequence );
                cubes = (CubeState*) &entry.cubes[0];
            }
            else
            {
                CORE_ASSERT( snapshot_sequence_buffer );
                auto entry = snapshot_sequence_buffer->Insert( sequence );
                CORE_ASSERT( entry );
                cubes = (CubeState*) &entry->cubes[0];
            }
            CORE_ASSERT( cubes );
        }
        else
        {
            if ( Stream::IsWriting )
            {
                CORE_ASSERT( quantized_snapshot_sliding_window );
                auto & entry = quantized_snapshot_sliding_window->Get( sequence );
                quantized_cubes = (QuantizedCubeState*) &entry.cubes[0];
            }
            else
            {
                CORE_ASSERT( quantized_snapshot_sequence_buffer );
                auto entry = quantized_snapshot_sequence_buffer->Insert( sequence );
                CORE_ASSERT( entry );
                quantized_cubes = (QuantizedCubeState*) &entry->cubes[0];
            }
            CORE_ASSERT( quantized_cubes );
        }

        switch ( compression_mode )
        {
            case COMPRESSION_MODE_UNCOMPRESSED:
            {
                for ( int i = 0; i < NumCubes; ++i )
                {
                    serialize_bool( stream, cubes[i].interacting );
                    serialize_vector( stream, cubes[i].position );
                    serialize_quaternion( stream, cubes[i].orientation );
                    serialize_vector( stream, cubes[i].linear_velocity );
                }
            }
            break;

            case COMPRESSION_MODE_ORIENTATION:
            {
                for ( int i = 0; i < NumCubes; ++i )
                {
                    serialize_bool( stream, cubes[i].interacting );
                    serialize_vector( stream, cubes[i].position );
                    serialize_compressed_quaternion( stream, cubes[i].orientation, 9 );
                    serialize_vector( stream, cubes[i].linear_velocity );
                }
            }
            break;

            case COMPRESSION_MODE_AT_REST:
            {
                for ( int i = 0; i < NumCubes; ++i )
                {
                    serialize_bool( stream, cubes[i].interacting );
                    serialize_vector( stream, cubes[i].position );
                    serialize_compressed_quaternion( stream, cubes[i].orientation, 9 );

                    bool at_rest;
                    if ( Stream::IsWriting )
                        at_rest = length_squared( cubes[i].linear_velocity ) <= 0.000001f;
                    serialize_bool( stream, at_rest );
                    if ( !at_rest )
                        serialize_vector( stream, cubes[i].linear_velocity );
                    else if ( Stream::IsReading )
                        cubes[i].linear_velocity = vectorial::vec3f::zero();
                }
            }
            break;

            case COMPRESSION_MODE_QUANTIZE_POSITION:
            {
                for ( int i = 0; i < NumCubes; ++i )
                {
                    serialize_bool( stream, quantized_cubes[i].interacting );
                    serialize_int( stream, quantized_cubes[i].position_x, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
                    serialize_int( stream, quantized_cubes[i].position_y, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
                    serialize_int( stream, quantized_cubes[i].position_z, 0, +QuantizedPositionBoundZ );
                    serialize_compressed_quaternion( stream, quantized_cubes[i].orientation, 9 );
                }
            }
            break;

            case COMPRESSION_MODE_DELTA_NOT_CHANGED:
            {
                CORE_ASSERT( quantized_initial_snapshot );

                QuantizedCubeState * quantized_base_cubes = nullptr;

                if ( initial )
                {
                    quantized_base_cubes = quantized_initial_snapshot->cubes;
                }
                else
                {
                    if ( Stream::IsWriting )
                    {
                        CORE_ASSERT( quantized_snapshot_sliding_window );
                        auto & entry = quantized_snapshot_sliding_window->Get( base_sequence );
                        quantized_base_cubes = (QuantizedCubeState*) &entry.cubes[0];
                    }
                    else
                    {
                        CORE_ASSERT( quantized_snapshot_sequence_buffer );
                        auto entry = quantized_snapshot_sequence_buffer->Find( base_sequence );
                        CORE_ASSERT( entry );
                        quantized_base_cubes = (QuantizedCubeState*) &entry->cubes[0];
                    }
                }

                for ( int i = 0; i < NumCubes; ++i )
                {
                    bool changed = false;

                    if ( Stream::IsWriting )
                        changed = quantized_cubes[i] != quantized_base_cubes[i];

                    serialize_bool( stream, changed );

                    if ( changed )
                    {
                        serialize_bool( stream, quantized_cubes[i].interacting );
                        serialize_int( stream, quantized_cubes[i].position_x, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
                        serialize_int( stream, quantized_cubes[i].position_y, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
                        serialize_int( stream, quantized_cubes[i].position_z, 0, +QuantizedPositionBoundZ );
                        serialize_compressed_quaternion( stream, quantized_cubes[i].orientation, 9 );
                    }
                    else if ( Stream::IsReading )
                    {
                        memcpy( &quantized_cubes[i], &quantized_base_cubes[i], sizeof( QuantizedCubeState ) );
                    }
                }
            }
            break;

            case COMPRESSION_MODE_DELTA_RELATIVE_POSITION:
            {
                CORE_ASSERT( quantized_initial_snapshot );

                QuantizedCubeState * quantized_base_cubes = nullptr;

                if ( initial )
                {
                    quantized_base_cubes = quantized_initial_snapshot->cubes;
                }
                else
                {
                    if ( Stream::IsWriting )
                    {
                        CORE_ASSERT( quantized_snapshot_sliding_window );
                        auto & entry = quantized_snapshot_sliding_window->Get( base_sequence );
                        quantized_base_cubes = (QuantizedCubeState*) &entry.cubes[0];
                    }
                    else
                    {
                        CORE_ASSERT( quantized_snapshot_sequence_buffer );
                        auto entry = quantized_snapshot_sequence_buffer->Find( base_sequence );
                        CORE_ASSERT( entry );
                        quantized_base_cubes = (QuantizedCubeState*) &entry->cubes[0];
                    }
                }

                for ( int i = 0; i < NumCubes; ++i )
                {
                    bool changed = false;

                    if ( Stream::IsWriting )
                        changed = quantized_cubes[i] != quantized_base_cubes[i];

                    serialize_bool( stream, changed );

                    if ( changed )
                    {
                        serialize_bool( stream, quantized_cubes[i].interacting );

                        bool relative_position_a;
                        bool relative_position_b;

                        const int RelativePositionBoundA = 63;
                        const int RelativePositionBoundB = 511;

                        if ( Stream::IsWriting )
                        {
                            relative_position_a = abs( quantized_cubes[i].position_x - quantized_base_cubes[i].position_x ) <= RelativePositionBoundA &&
                                                  abs( quantized_cubes[i].position_y - quantized_base_cubes[i].position_y ) <= RelativePositionBoundA &&
                                                  abs( quantized_cubes[i].position_z - quantized_base_cubes[i].position_z ) <= RelativePositionBoundA;

                            relative_position_b = abs( quantized_cubes[i].position_x - quantized_base_cubes[i].position_x ) <= RelativePositionBoundB &&
                                                  abs( quantized_cubes[i].position_y - quantized_base_cubes[i].position_y ) <= RelativePositionBoundB &&
                                                  abs( quantized_cubes[i].position_z - quantized_base_cubes[i].position_z ) <= RelativePositionBoundB;
                        }

                        serialize_bool( stream, relative_position_a );

                        if ( relative_position_a )
                        {
                            int offset_x, offset_y, offset_z;

                            if ( Stream::IsWriting )
                            {
                                offset_x = quantized_cubes[i].position_x - quantized_base_cubes[i].position_x;
                                offset_y = quantized_cubes[i].position_y - quantized_base_cubes[i].position_y;
                                offset_z = quantized_cubes[i].position_z - quantized_base_cubes[i].position_z;
                            }

                            serialize_int( stream, offset_x, -RelativePositionBoundA, +RelativePositionBoundA );
                            serialize_int( stream, offset_y, -RelativePositionBoundA, +RelativePositionBoundA );
                            serialize_int( stream, offset_z, -RelativePositionBoundA, +RelativePositionBoundA );

                            quantized_cubes[i].position_x = quantized_base_cubes[i].position_x + offset_x;
                            quantized_cubes[i].position_y = quantized_base_cubes[i].position_y + offset_y;
                            quantized_cubes[i].position_z = quantized_base_cubes[i].position_z + offset_z;
                        }
                        else
                        {
                            serialize_bool( stream, relative_position_b );

                            if ( relative_position_b )
                            {
                                int offset_x, offset_y, offset_z;

                                if ( Stream::IsWriting )
                                {
                                    offset_x = quantized_cubes[i].position_x - quantized_base_cubes[i].position_x;
                                    offset_y = quantized_cubes[i].position_y - quantized_base_cubes[i].position_y;
                                    offset_z = quantized_cubes[i].position_z - quantized_base_cubes[i].position_z;
                                }

                                serialize_int( stream, offset_x, -RelativePositionBoundB, +RelativePositionBoundB );
                                serialize_int( stream, offset_y, -RelativePositionBoundB, +RelativePositionBoundB );
                                serialize_int( stream, offset_z, -RelativePositionBoundB, +RelativePositionBoundB );

                                quantized_cubes[i].position_x = quantized_base_cubes[i].position_x + offset_x;
                                quantized_cubes[i].position_y = quantized_base_cubes[i].position_y + offset_y;
                                quantized_cubes[i].position_z = quantized_base_cubes[i].position_z + offset_z;
                            }
                            else
                            {
                                serialize_int( stream, quantized_cubes[i].position_x, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
                                serialize_int( stream, quantized_cubes[i].position_y, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
                                serialize_int( stream, quantized_cubes[i].position_z, 0, +QuantizedPositionBoundZ );

                                if ( Stream::IsReading )
                                {
                                    quantized_cubes[i].interacting = false;
                                }
                            }
                        }

                        serialize_compressed_quaternion( stream, quantized_cubes[i].orientation, 9 );
                    }
                    else if ( Stream::IsReading )
                    {
                        memcpy( &quantized_cubes[i], &quantized_base_cubes[i], sizeof( QuantizedCubeState ) );
                    }
                }
            }
            break;

#if 0

            case COMPRESSION_MODE_DELTA_POSITION_GRID:
            {
                CORE_ASSERT( initial_snapshot );

                CubeState * base_cubes = nullptr;

                if ( initial )
                {
                    base_cubes = initial_snapshot->cubes;
                }
                else
                {
                    // todo: track down desync between left and right snapshot buffers

//                    if ( Stream::IsWriting )
                    {
                        CORE_ASSERT( snapshot_sliding_window );
                        auto & entry = snapshot_sliding_window->Get( base_sequence );
                        base_cubes = (CubeState*) &entry.cubes[0];
                    }
                    /*
                    else
                    {
                        CORE_ASSERT( snapshot_sequence_buffer );
                        auto entry = snapshot_sequence_buffer->Find( base_sequence );
                        CORE_ASSERT( entry );
                        base_cubes = (CubeState*) &entry->cubes[0];
                    }
                    */
                }

                GridCubeState grid_cubes[NumCubes];
                GridCubeState base_grid_cubes[NumCubes];
                for ( int i = 0; i < NumCubes; ++i )
                {
                    grid_cubes[i].Load( cubes[i] );
                    base_grid_cubes[i].Load( base_cubes[i] );
                }

                for ( int i = 0; i < NumCubes; ++i )
                {
                    bool changed;

                    if ( Stream::IsWriting )
                        changed = grid_cubes[i] != base_grid_cubes[i];

                    serialize_bool( stream, changed );

                    if ( changed )
                    {
                        serialize_bool( stream, grid_cubes[i].interacting );

                        const int num_grid_cells_xy = int( PositionBoundXY * 2 ) / GridCubeSize;
                        const int num_grid_cells_z = int( PositionBoundZ ) / GridCubeSize;

                        bool grid_cell_changed;

                        if ( Stream::IsWriting )
                        {
                            grid_cell_changed = grid_cubes[i].ix != base_grid_cubes[i].ix ||
                                                grid_cubes[i].iy != base_grid_cubes[i].iy ||
                                                grid_cubes[i].iz != base_grid_cubes[i].iz;
                        }

                        serialize_bool( stream, grid_cell_changed );

                        if ( grid_cell_changed )
                        {
                            serialize_int( stream, grid_cubes[i].ix, 0, num_grid_cells_xy - 1 );
                            serialize_int( stream, grid_cubes[i].iy, 0, num_grid_cells_xy - 1 );
                            serialize_int( stream, grid_cubes[i].iz, 0, num_grid_cells_z - 1 );
                        }
                        else
                        {
                            if ( Stream::IsReading )
                            {
                                grid_cubes[i].ix = base_grid_cubes[i].ix;
                                grid_cubes[i].iy = base_grid_cubes[i].iy;
                                grid_cubes[i].iz = base_grid_cubes[i].iz;
                            }

                            // todo: track down difference between left and right snapshot buffers!

                            /*
                            if ( Stream::IsWriting )
                            {
                                serialize_int( stream, grid_cubes[i].ix, 0, num_grid_cells_xy - 1 );
                                serialize_int( stream, grid_cubes[i].iy, 0, num_grid_cells_xy - 1 );
                                serialize_int( stream, grid_cubes[i].iz, 0, num_grid_cells_z - 1 );
                            }
                            else
                            {
                                grid_cubes[i].ix = base_grid_cubes[i].ix;
                                grid_cubes[i].iy = base_grid_cubes[i].iy;
                                grid_cubes[i].iz = base_grid_cubes[i].iz;

                                int ix,iy,iz;
                                serialize_int( stream, ix, 0, num_grid_cells_xy - 1 );
                                serialize_int( stream, iy, 0, num_grid_cells_xy - 1 );
                                serialize_int( stream, iz, 0, num_grid_cells_z - 1 );

                                if ( ix != grid_cubes[i].ix ||
                                     iy != grid_cubes[i].iy ||
                                     iz != grid_cubes[i].iz )
                                {
                                    printf( "position grid mismatch: expected (%d,%d,%d), got (%d,%d,%d)\n",
                                        ix, iy, iz, grid_cubes[i].ix, grid_cubes[i].iy, grid_cubes[i].iz );
                                }

                                CORE_ASSERT( ix == grid_cubes[i].ix );
                                CORE_ASSERT( iy == grid_cubes[i].iy );
                                CORE_ASSERT( iz == grid_cubes[i].iz );
                            }
                            */
                        }
                        
                        serialize_compressed_vector( stream, grid_cubes[i].local_position, local_position_min, local_position_max, 0.001 );
                        
                        serialize_compressed_quaternion( stream, grid_cubes[i].orientation, 9 );
                    }
                    else if ( Stream::IsReading )
                    {
                        memcpy( &grid_cubes[i], &base_grid_cubes[i], sizeof( GridCubeState ) );
                    }
                }

                if ( Stream::IsReading )
                {
                    for ( int i = 0; i < NumCubes; ++i )
                        grid_cubes[i].Save( cubes[i] );
                }
            }
            break;

            case COMPRESSION_MODE_DELTA_POSITION_RELATIVE:
            {
                CORE_ASSERT( initial_snapshot );

                CubeState * base_cubes = nullptr;

                if ( initial )
                {
                    base_cubes = initial_snapshot->cubes;
                }
                else
                {
                    // todo: track down desync between left and right snapshot buffers

//                    if ( Stream::IsWriting )
                    {
                        CORE_ASSERT( snapshot_sliding_window );
                        auto & entry = snapshot_sliding_window->Get( base_sequence );
                        base_cubes = (CubeState*) &entry.cubes[0];
                    }
                    /*
                    else
                    {
                        CORE_ASSERT( snapshot_sequence_buffer );
                        auto entry = snapshot_sequence_buffer->Find( base_sequence );
                        CORE_ASSERT( entry );
                        base_cubes = (CubeState*) &entry->cubes[0];
                    }
                    */
                }

                GridCubeState grid_cubes[NumCubes];
                GridCubeState base_grid_cubes[NumCubes];
                for ( int i = 0; i < NumCubes; ++i )
                {
                    grid_cubes[i].Load( cubes[i] );
                    base_grid_cubes[i].Load( base_cubes[i] );
                }

                for ( int i = 0; i < NumCubes; ++i )
                {
                    bool changed;

                    if ( Stream::IsWriting )
                        changed = grid_cubes[i] != base_grid_cubes[i];

                    serialize_bool( stream, changed );

                    if ( changed )
                    {
                        serialize_bool( stream, grid_cubes[i].interacting );

                        const int num_grid_cells_xy = int( PositionBoundXY * 2 ) / GridCubeSize;
                        const int num_grid_cells_z = int( PositionBoundZ ) / GridCubeSize;

                        bool grid_cell_changed;

                        if ( Stream::IsWriting )
                        {
                            grid_cell_changed = grid_cubes[i].ix != base_grid_cubes[i].ix ||
                                                grid_cubes[i].iy != base_grid_cubes[i].iy ||
                                                grid_cubes[i].iz != base_grid_cubes[i].iz;
                        }

                        serialize_bool( stream, grid_cell_changed );

                        if ( grid_cell_changed )
                        {
                            serialize_int( stream, grid_cubes[i].ix, 0, num_grid_cells_xy - 1 );
                            serialize_int( stream, grid_cubes[i].iy, 0, num_grid_cells_xy - 1 );
                            serialize_int( stream, grid_cubes[i].iz, 0, num_grid_cells_z - 1 );
                        }
                        else
                        {
                            if ( Stream::IsReading )
                            {
                                grid_cubes[i].ix = base_grid_cubes[i].ix;
                                grid_cubes[i].iy = base_grid_cubes[i].iy;
                                grid_cubes[i].iz = base_grid_cubes[i].iz;
                            }
                        }
                        
                        bool relative_position = false;

                        const float RelativePositionThreshold = 0.25;

                        if ( Stream::IsWriting )
                        {
                            const float dx = grid_cubes[i].local_position.x() - base_grid_cubes[i].local_position.x();
                            const float dy = grid_cubes[i].local_position.y() - base_grid_cubes[i].local_position.y();
                            const float dz = grid_cubes[i].local_position.z() - base_grid_cubes[i].local_position.z();

                            if ( fabs( dx ) < RelativePositionThreshold && 
                                 fabs( dy ) < RelativePositionThreshold &&
                                 fabs( dz ) < RelativePositionThreshold )
                            {
                                relative_position = true;
                            }
                        }

                        serialize_bool( stream, relative_position );

                        if ( relative_position )
                        {
                            vectorial::vec3f offset;

                            if ( Stream::IsWriting )
                                offset = grid_cubes[i].local_position - base_grid_cubes[i].local_position;

                            serialize_compressed_vector( stream, offset, RelativePositionThreshold, 0.001 );

                            if ( Stream::IsReading )
                                grid_cubes[i].local_position = base_grid_cubes[i].local_position + offset;
                        }
                        else
                        {
                            serialize_compressed_vector( stream, grid_cubes[i].local_position, local_position_min, local_position_max, 0.001 );
                        }
                        
                        serialize_compressed_quaternion( stream, grid_cubes[i].orientation, 9 );
                    }
                    else if ( Stream::IsReading )
                    {
                        memcpy( &grid_cubes[i], &base_grid_cubes[i], sizeof( GridCubeState ) );
                    }
                }

                if ( Stream::IsReading )
                {
                    for ( int i = 0; i < NumCubes; ++i )
                        grid_cubes[i].Save( cubes[i] );
                }
            }
            break;

            /*
            case COMPRESSION_MODE_DELTA_ABSOLUTE_INDICES:
            {
                CORE_ASSERT( initial_snapshot );

                CubeState * base_cubes = nullptr;

                if ( initial )
                {
                    base_cubes = initial_snapshot->cubes;
                }
                else
                {
                    if ( Stream::IsWriting )
                    {
                        CORE_ASSERT( snapshot_sliding_window );
                        auto & entry = snapshot_sliding_window->Get( base_sequence );
                        base_cubes = (CubeState*) &entry.cubes[0];
                    }
                    else
                    {
                        CORE_ASSERT( snapshot_sequence_buffer );
                        auto entry = snapshot_sequence_buffer->Find( base_sequence );
                        CORE_ASSERT( entry );
                        base_cubes = (CubeState*) &entry->cubes[0];
                    }
                }

                const int MaxIndex = 89;

                int num_changed = 0;
                bool use_indices = false;
                bool changed[NumCubes];
                if ( Stream::IsWriting )
                {
                    for ( int i = 0; i < NumCubes; ++i )
                    {
                        changed[i] = cubes[i] != base_cubes[i];
                        if ( changed[i] )
                            num_changed++;
                    }
                    if ( num_changed < MaxIndex )
                        use_indices = true;
                }

                serialize_bool( stream, use_indices );

                if ( use_indices )
                {
                    serialize_int( stream, num_changed, 0, MaxIndex + 1 );

                    if ( Stream::IsWriting )
                    {
                        int num_written = 0;

                        for ( int i = 0; i < NumCubes; ++i )
                        {
                            if ( changed[i] )
                            {
                                serialize_int( stream, i, 0, NumCubes - 1 );

                                serialize_bool( stream, cubes[i].interacting );

                                serialize_compressed_vector( stream, cubes[i].position, position_min, position_max, 0.001 );
                                
                                serialize_compressed_quaternion( stream, cubes[i].orientation, 9 );

                                num_written++;
                            }
                        }

                        CORE_ASSERT( num_written == num_changed );
                    }
                    else
                    {
                        memset( changed, 0, sizeof( changed ) );

                        for ( int i = 0; i < num_changed; ++i )
                        {
                            int index;
                            serialize_int( stream, index, 0, NumCubes - 1 );

                            serialize_bool( stream, cubes[index].interacting );

                            serialize_compressed_vector( stream, cubes[index].position, position_min, position_max, 0.001 );
                            
                            serialize_compressed_quaternion( stream, cubes[index].orientation, 9 );

                            changed[index] = true;
                        }

                        for ( int i = 0; i < NumCubes; ++i )
                        {
                            if ( !changed[i] )
                                memcpy( &cubes[i], &base_cubes[i], sizeof( CubeState ) );
                        }
                    }
                }
                else
                {
                    for ( int i = 0; i < NumCubes; ++i )
                    {
                        serialize_bool( stream, changed[i] );

                        if ( changed[i] )
                        {
                            serialize_bool( stream, cubes[i].interacting );

                            serialize_compressed_vector( stream, cubes[i].position, position_min, position_max, 0.001 );
                            
                            serialize_compressed_quaternion( stream, cubes[i].orientation, 9 );
                        }
                        else if ( Stream::IsReading )
                        {
                            memcpy( &cubes[i], &base_cubes[i], sizeof( CubeState ) );
                        }
                    }
                }
            }
            break;

            case COMPRESSION_MODE_DELTA_RELATIVE_INDICES:
            {
                CORE_ASSERT( initial_snapshot );

                CubeState * base_cubes = nullptr;

                if ( initial )
                {
                    base_cubes = initial_snapshot->cubes;
                }
                else
                {
                    if ( Stream::IsWriting )
                    {
                        CORE_ASSERT( snapshot_sliding_window );
                        auto & entry = snapshot_sliding_window->Get( base_sequence );
                        base_cubes = (CubeState*) &entry.cubes[0];
                    }
                    else
                    {
                        CORE_ASSERT( snapshot_sequence_buffer );
                        auto entry = snapshot_sequence_buffer->Find( base_sequence );
                        CORE_ASSERT( entry );
                        base_cubes = (CubeState*) &entry->cubes[0];
                    }
                }

                const int MaxIndex = 126;

                int num_changed = 0;
                bool use_indices = false;
                bool changed[NumCubes];
                if ( Stream::IsWriting )
                {
                    for ( int i = 0; i < NumCubes; ++i )
                    {
                        changed[i] = cubes[i] != base_cubes[i];
                        if ( changed[i] )
                            num_changed++;
                    }
                    if ( num_changed < MaxIndex )
                        use_indices = true;
                }

                serialize_bool( stream, use_indices );

                if ( use_indices )
                {
                    serialize_int( stream, num_changed, 0, MaxIndex + 1 );

                    if ( Stream::IsWriting )
                    {
                        int num_written = 0;

                        bool first = true;
                        int previous_index = 0;

                        for ( int i = 0; i < NumCubes; ++i )
                        {
                            if ( changed[i] )
                            {
                                if ( first )
                                {
                                    serialize_int( stream, i, 0, NumCubes - 1 );
                                    first = false;
                                }
                                else
                                {   
                                    serialize_index_relative( stream, previous_index, i );
                                }

                                serialize_bool( stream, cubes[i].interacting );

                                serialize_compressed_vector( stream, cubes[i].position, position_min, position_max, 0.001 );
                                
                                serialize_compressed_quaternion( stream, cubes[i].orientation, 9 );

                                num_written++;

                                previous_index = i;
                            }
                        }

                        CORE_ASSERT( num_written == num_changed );
                    }
                    else
                    {
                        memset( changed, 0, sizeof( changed ) );

                        int previous_index = 0;

                        for ( int i = 0; i < num_changed; ++i )
                        {
                            int index;
                            if ( i == 0 )
                                serialize_int( stream, index, 0, NumCubes - 1 );
                            else                                
                                serialize_index_relative( stream, previous_index, index );

                            serialize_bool( stream, cubes[index].interacting );

                            serialize_compressed_vector( stream, cubes[index].position, position_min, position_max, 0.001 );
                            
                            serialize_compressed_quaternion( stream, cubes[index].orientation, 9 );

                            changed[index] = true;

                            previous_index = index;
                        }

                        for ( int i = 0; i < NumCubes; ++i )
                        {
                            if ( !changed[i] )
                                memcpy( &cubes[i], &base_cubes[i], sizeof( CubeState ) );
                        }
                    }
                }
                else
                {
                    for ( int i = 0; i < NumCubes; ++i )
                    {
                        serialize_bool( stream, changed[i] );

                        if ( changed[i] )
                        {
                            serialize_bool( stream, cubes[i].interacting );

                            serialize_compressed_vector( stream, cubes[i].position, position_min, position_max, 0.001 );
                            
                            serialize_compressed_quaternion( stream, cubes[i].orientation, 9 );
                        }
                        else if ( Stream::IsReading )
                        {
                            memcpy( &cubes[i], &base_cubes[i], sizeof( CubeState ) );
                        }
                    }
                }
            }
            break;
            */

#endif

            default:
                break;
        }
    }
};

struct CompressionAckPacket : public protocol::Packet
{
    uint16_t ack;

    CompressionAckPacket() : Packet( COMPRESSION_ACK_PACKET )
    {
        ack = 0;
    }

    PROTOCOL_SERIALIZE_OBJECT( stream )
    {
        serialize_uint16( stream, ack );
    }
};

class SnapshotPacketFactory : public protocol::PacketFactory
{
    core::Allocator * m_allocator;

public:

    SnapshotPacketFactory( core::Allocator & allocator )
        : PacketFactory( allocator, COMPRESSION_NUM_PACKETS )
    {
        m_allocator = &allocator;
    }

protected:

    protocol::Packet * CreateInternal( int type )
    {
        switch ( type )
        {
            case COMPRESSION_SNAPSHOT_PACKET:   return CORE_NEW( *m_allocator, CompressionSnapshotPacket );
            case COMPRESSION_ACK_PACKET:        return CORE_NEW( *m_allocator, CompressionAckPacket );
            default:
                return nullptr;
        }
    }
};

struct CompressionInternal
{
    CompressionInternal( core::Allocator & allocator, const SnapshotModeData & mode_data ) 
        : packet_factory( allocator ), interpolation_buffer( allocator, mode_data )
    {
        this->allocator = &allocator;
        network::SimulatorConfig networkSimulatorConfig;
        snapshot_sliding_window = CORE_NEW( allocator, SnapshotSlidingWindow, allocator, MaxSnapshots );
        snapshot_sequence_buffer = CORE_NEW( allocator, SnapshotSequenceBuffer, allocator, MaxSnapshots );
        quantized_snapshot_sliding_window = CORE_NEW( allocator, QuantizedSnapshotSlidingWindow, allocator, MaxSnapshots );
        quantized_snapshot_sequence_buffer = CORE_NEW( allocator, QuantizedSnapshotSequenceBuffer, allocator, MaxSnapshots );
        networkSimulatorConfig.packetFactory = &packet_factory;
        networkSimulatorConfig.maxPacketSize = MaxPacketSize;
        network_simulator = CORE_NEW( allocator, network::Simulator, networkSimulatorConfig );
        context[0] = snapshot_sliding_window;
        context[1] = snapshot_sequence_buffer;
        context[2] = quantized_snapshot_sliding_window;
        context[3] = quantized_snapshot_sequence_buffer;
        context[4] = &quantized_initial_snapshot;
        network_simulator->SetContext( context );
        Reset( mode_data );
    }

    ~CompressionInternal()
    {
        CORE_ASSERT( network_simulator );
        typedef network::Simulator NetworkSimulator;
        CORE_DELETE( *allocator, NetworkSimulator, network_simulator );
        CORE_DELETE( *allocator, SnapshotSlidingWindow, snapshot_sliding_window );
        CORE_DELETE( *allocator, SnapshotSequenceBuffer, snapshot_sequence_buffer );
        CORE_DELETE( *allocator, QuantizedSnapshotSlidingWindow, quantized_snapshot_sliding_window );
        CORE_DELETE( *allocator, QuantizedSnapshotSequenceBuffer, quantized_snapshot_sequence_buffer );
        network_simulator = nullptr;
        snapshot_sliding_window = nullptr;
        snapshot_sequence_buffer = nullptr;
        quantized_snapshot_sliding_window = nullptr;
        quantized_snapshot_sequence_buffer = nullptr;
    }

    void Reset( const SnapshotModeData & mode_data )
    {
        interpolation_buffer.Reset();
        network_simulator->Reset();
        network_simulator->ClearStates();
        network_simulator->AddState( { mode_data.latency, mode_data.jitter, mode_data.packet_loss } );
        snapshot_sliding_window->Reset();
        snapshot_sequence_buffer->Reset();
        quantized_snapshot_sliding_window->Reset();
        quantized_snapshot_sequence_buffer->Reset();
        send_sequence = 0;
        recv_sequence = 0;
        send_accumulator = 1.0f;
        received_ack = false;
    }

    core::Allocator * allocator;
    uint16_t send_sequence;
    uint16_t recv_sequence;
    bool received_ack;
    float send_accumulator;
    const void * context[6];
    network::Simulator * network_simulator;
    SnapshotSlidingWindow * snapshot_sliding_window;
    SnapshotSequenceBuffer * snapshot_sequence_buffer;
    QuantizedSnapshotSlidingWindow * quantized_snapshot_sliding_window;
    QuantizedSnapshotSequenceBuffer * quantized_snapshot_sequence_buffer;
    SnapshotPacketFactory packet_factory;
    SnapshotInterpolationBuffer interpolation_buffer;
    QuantizedSnapshot quantized_initial_snapshot;
};

CompressionDemo::CompressionDemo( core::Allocator & allocator )
{
    InitCompressionModes();
    m_allocator = &allocator;
    m_internal = nullptr;
    m_settings = CORE_NEW( *m_allocator, CubesSettings );
    m_compression = CORE_NEW( *m_allocator, CompressionInternal, *m_allocator, compression_mode_data[GetMode()] );
}

CompressionDemo::~CompressionDemo()
{
    Shutdown();
    CORE_DELETE( *m_allocator, CompressionInternal, m_compression );
    CORE_DELETE( *m_allocator, CubesSettings, m_settings );
    m_compression = nullptr;
    m_settings = nullptr;
    m_allocator = nullptr;
}

bool CompressionDemo::Initialize()
{
    if ( m_internal )
        Shutdown();

    m_internal = CORE_NEW( *m_allocator, CubesInternal );    

    CubesConfig config;
    
    config.num_simulations = 1;
    config.num_views = 2;

    m_internal->Initialize( *m_allocator, config, m_settings );

    auto game_instance = m_internal->GetGameInstance( 0 );

    bool result = GetQuantizedSnapshot( game_instance, m_compression->quantized_initial_snapshot );
    CORE_ASSERT( result );

    return true;
}

void CompressionDemo::Shutdown()
{
    CORE_ASSERT( m_allocator );

    CORE_ASSERT( m_compression );
    m_compression->Reset( compression_mode_data[GetMode()] );

    if ( m_internal )
    {
        m_internal->Free( *m_allocator );
        CORE_DELETE( *m_allocator, CubesInternal, m_internal );
        m_internal = nullptr;
    }
}

void CompressionDemo::Update()
{
    CubesUpdateConfig update_config;

    auto local_input = m_internal->GetLocalInput();

    // setup left simulation to update one frame with local input

    update_config.sim[0].num_frames = 1;
    update_config.sim[0].frame_input[0] = local_input;

    // send a snapshot packet to the right simulation

    m_compression->send_accumulator += global.timeBase.deltaTime;

    if ( m_compression->send_accumulator >= 1.0f / compression_mode_data[GetMode()].send_rate )
    {
        m_compression->send_accumulator = 0.0f;   

        auto game_instance = m_internal->GetGameInstance( 0 );

        auto snapshot_packet = (CompressionSnapshotPacket*) m_compression->packet_factory.Create( COMPRESSION_SNAPSHOT_PACKET );

        if ( GetMode() < COMPRESSION_MODE_QUANTIZE_POSITION )
        {
            snapshot_packet->sequence = m_compression->send_sequence++;
            snapshot_packet->base_sequence = m_compression->snapshot_sliding_window->GetAck() + 1;
            snapshot_packet->initial = !m_compression->received_ack;

            snapshot_packet->compression_mode = GetMode();

            uint16_t sequence;

            auto & snapshot = m_compression->snapshot_sliding_window->Insert( sequence );

            if ( GetSnapshot( game_instance, snapshot ) )
                m_compression->network_simulator->SendPacket( network::Address( "::1", RightPort ), snapshot_packet );
            else
                m_compression->packet_factory.Destroy( snapshot_packet );
        }
        else
        {
            snapshot_packet->sequence = m_compression->send_sequence++;
            snapshot_packet->base_sequence = m_compression->quantized_snapshot_sliding_window->GetAck() + 1;
            snapshot_packet->initial = !m_compression->received_ack;

            snapshot_packet->compression_mode = GetMode();

            uint16_t sequence;

            auto & snapshot = m_compression->quantized_snapshot_sliding_window->Insert( sequence );

            if ( GetQuantizedSnapshot( game_instance, snapshot ) )
                m_compression->network_simulator->SendPacket( network::Address( "::1", RightPort ), snapshot_packet );
            else
                m_compression->packet_factory.Destroy( snapshot_packet );
        }
    }

    // update the network simulator

    m_compression->network_simulator->Update( global.timeBase );

    // receive packets from the simulator (with latency, packet loss and jitter applied...)

    bool received_snapshot_this_frame = false;
    uint16_t ack_sequence = 0;

    while ( true )
    {
        auto packet = m_compression->network_simulator->ReceivePacket();
        if ( !packet )
            break;

        const auto port = packet->GetAddress().GetPort();
        const auto type = packet->GetType();

        if ( type == COMPRESSION_SNAPSHOT_PACKET && port == RightPort )
        {
            auto snapshot_packet = (CompressionSnapshotPacket*) packet;
            if ( GetMode() < COMPRESSION_MODE_QUANTIZE_POSITION )
            {
                Snapshot * snapshot = m_compression->snapshot_sequence_buffer->Find( snapshot_packet->sequence );
                CORE_ASSERT( snapshot );
                m_compression->interpolation_buffer.AddSnapshot( global.timeBase.time, snapshot_packet->sequence, snapshot->cubes );
            }
            else
            {
                QuantizedSnapshot * quantized_snapshot = m_compression->quantized_snapshot_sequence_buffer->Find( snapshot_packet->sequence );
                CORE_ASSERT( quantized_snapshot );
                Snapshot snapshot;
                for ( int i = 0; i < NumCubes; ++i )
                    quantized_snapshot->cubes[i].Save( snapshot.cubes[i] );
                m_compression->interpolation_buffer.AddSnapshot( global.timeBase.time, snapshot_packet->sequence, snapshot.cubes );
            }
            if ( !received_snapshot_this_frame || ( received_snapshot_this_frame && core::sequence_greater_than( snapshot_packet->sequence, ack_sequence ) ) )
            {
                received_snapshot_this_frame = true;
                ack_sequence = snapshot_packet->sequence;
            }
        }
        else if ( type == COMPRESSION_ACK_PACKET && port == LeftPort )
        {
            auto ack_packet = (CompressionAckPacket*) packet;

            if ( GetMode() < COMPRESSION_MODE_QUANTIZE_POSITION )
            {
                m_compression->snapshot_sliding_window->Ack( ack_packet->ack - 1 );
                m_compression->received_ack = true;
            }
            else
            {
                m_compression->quantized_snapshot_sliding_window->Ack( ack_packet->ack - 1 );
                m_compression->received_ack = true;
            }
        }

        m_compression->packet_factory.Destroy( packet );
    }

    // if any snapshots packets were received this frame, send an ack packet back to the left simulation

    if ( received_snapshot_this_frame )
    {
        auto ack_packet = (CompressionAckPacket*) m_compression->packet_factory.Create( COMPRESSION_ACK_PACKET );
        ack_packet->ack = ack_sequence;
        m_compression->network_simulator->SetBandwidthExclude( true );
        m_compression->network_simulator->SendPacket( network::Address( "::1", LeftPort ), ack_packet );
        m_compression->network_simulator->SetBandwidthExclude( false );
    }

    // if we are an an interpolation mode, we need to grab the view updates for the right side from the interpolation buffer

    int num_object_updates = 0;

    view::ObjectUpdate object_updates[NumCubes];

    m_compression->interpolation_buffer.GetViewUpdate( compression_mode_data[GetMode()], global.timeBase.time, object_updates, num_object_updates );

    if ( num_object_updates > 0 )
        m_internal->view[1].objects.UpdateObjects( object_updates, num_object_updates );
    else if ( m_compression->interpolation_buffer.interpolating )
        printf( "no snapshot to interpolate towards!\n" );

    // run the simulation

    m_internal->Update( update_config );
}

bool CompressionDemo::Clear()
{
    return m_internal->Clear();
}

void CompressionDemo::Render()
{
    CubesRenderConfig render_config;

    render_config.render_mode = CUBES_RENDER_SPLITSCREEN;

    m_internal->Render( render_config );

    const float bandwidth = m_compression->network_simulator->GetBandwidth();

    char bandwidth_string[256];
    if ( bandwidth < 1024 )
        snprintf( bandwidth_string, (int) sizeof( bandwidth_string ), "Bandwidth: %d kbps", (int) bandwidth );
    else
        snprintf( bandwidth_string, (int) sizeof( bandwidth_string ), "Bandwidth: %.2f mbps", bandwidth / 1000 );

    Font * font = global.fontManager->GetFont( "Bandwidth" );
    if ( font )
    {
        const float text_x = ( global.displayWidth - font->GetTextWidth( bandwidth_string ) ) / 2;
        const float text_y = 5;
        font->Begin();
        font->DrawText( text_x, text_y, bandwidth_string, Color( 0.27f,0.81f,1.0f ) );
        font->End();
    }
}

bool CompressionDemo::KeyEvent( int key, int scancode, int action, int mods )
{
    return m_internal->KeyEvent( key, scancode, action, mods );
}

bool CompressionDemo::CharEvent( unsigned int code )
{
    // ...

    return false;
}

int CompressionDemo::GetNumModes() const
{
    return COMPRESSION_NUM_MODES;
}

const char * CompressionDemo::GetModeDescription( int mode ) const
{
    return compression_mode_descriptions[mode];
}

#endif // #ifdef CLIENT
