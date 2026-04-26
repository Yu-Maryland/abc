/**CFile****************************************************************

  FileName    [abcStmap_67.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Versioned AutoEDA standard-cell mapper experiments.]

  Synopsis    [stmap67 command.]

***********************************************************************/

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "misc/extra/extra.h"
#include <string.h>

ABC_NAMESPACE_IMPL_START

/**Function*************************************************************

  Synopsis    [Runs the stmap67 mapper diagnostic.]

  Description [stmap67 keeps the reviewed stmap65 mapping policy unchanged
  and records area-cap-passing near-threshold strong-node load-drop candidates
  so the downstream stime call can report whether they remain loaded or
  timing-critical after buffering and sizing.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CommandStmap67( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    Abc_Ntk_t * pNtkRes;
    int i, RetValue;
    extern int Abc_CommandStmap65( Abc_Frame_t * pAbc, int argc, char ** argv );
    extern void Map_Stmap66SetNearStrongNodeLoadDropDiag( int fEnable );
    extern void Map_Stmap67SetNearStrongNodeWitnessDiag( int fEnable );
    extern void Abc_Stmap64EnableFinalWitnessScope( Abc_Ntk_t * pNtk, const char * pLabel );

    for ( i = 1; i < argc; i++ )
    {
        if ( !strcmp( argv[i], "-h" ) )
            goto usage;
    }

    printf( "stmap67 diagnostic: inherited-policy = stmap65  near-strong-node-load-drop = 1  final-stime-witness = 1  node-pressure-window = [1.15,1.25)  strong-node-threshold = 1.25\n" );
    Map_Stmap66SetNearStrongNodeLoadDropDiag( 1 );
    Map_Stmap67SetNearStrongNodeWitnessDiag( 1 );
    RetValue = Abc_CommandStmap65( pAbc, argc, argv );
    Map_Stmap67SetNearStrongNodeWitnessDiag( 0 );
    Map_Stmap66SetNearStrongNodeLoadDropDiag( 0 );
    if ( RetValue == 0 )
    {
        pNtkRes = Abc_FrameReadNtk( pAbc );
        Abc_Stmap64EnableFinalWitnessScope( pNtkRes, "stmap67" );
    }
    return RetValue;

usage:
    Abc_Print( -2, "usage: stmap67 [-DABFSG float] [-M num] [-arspfuovh]\n" );
    Abc_Print( -2, "\t              performs stmap67 bounded-pressure mapping with downstream near-threshold witness diagnostics\n" );
    Abc_Print( -2, "\t-D float : sets the global required times [default = not used]\n" );
    Abc_Print( -2, "\t-A float : \"area multiplier\" to bias gate selection [default = 0.00]\n" );
    Abc_Print( -2, "\t-B float : \"delay multiplier\" to bias gate selection [default = 0.00]\n" );
    Abc_Print( -2, "\t-F float : the logarithmic fanout delay parameter [default = 0.00]\n" );
    Abc_Print( -2, "\t-S float : the slew parameter used to generate the library [default = 0.00]\n" );
    Abc_Print( -2, "\t-G float : the SCL/genlib gain parameter used to generate the library [default = 250.00]\n" );
    Abc_Print( -2, "\t-M num   : skip gate classes whose size is less than this [default = 0]\n" );
    Abc_Print( -2, "\t-a       : toggles area-only mapping [default = no]\n" );
    Abc_Print( -2, "\t-r       : toggles area recovery [default = yes]\n" );
    Abc_Print( -2, "\t-s       : toggles sweep after mapping [default = no]\n" );
    Abc_Print( -2, "\t-p       : optimizes power by minimizing switching [default = no]\n" );
    Abc_Print( -2, "\t-f       : disables the bounded-pressure SCL feedback mapper mode [default = yes]\n" );
    Abc_Print( -2, "\t-u       : use standard-cell profile [default = no]\n" );
    Abc_Print( -2, "\t-o       : toggles using buffers to decouple combinational outputs [default = no]\n" );
    Abc_Print( -2, "\t-v       : toggles verbose output [default = no]\n" );
    Abc_Print( -2, "\t-h       : print the command usage\n" );
    return 1;
}

ABC_NAMESPACE_IMPL_END
