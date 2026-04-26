/**CFile****************************************************************

  FileName    [abcStmap_73.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Versioned AutoEDA standard-cell mapper experiments.]

  Synopsis    [stmap73 command.]

***********************************************************************/

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "misc/extra/extra.h"
#include <string.h>

ABC_NAMESPACE_IMPL_START

/**Function*************************************************************

  Synopsis    [Runs the stmap73 accepted-pressure witness diagnostic.]

  Description [stmap73 keeps the reviewed stmap65 mapping policy unchanged
  and records accepted pressure-assisted mode-57 mapper choices as downstream
  final witnesses.  The diagnostic checks whether accepted pressure-guided
  choices, rather than closed blocker classes, explain final stime critical
  paths before proposing another mapper policy change.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CommandStmap73( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    Abc_Ntk_t * pNtkRes;
    int i, RetValue;
    extern int Abc_CommandStmap65( Abc_Frame_t * pAbc, int argc, char ** argv );
    extern void Map_Stmap73SetAcceptedPressureWitnessDiag( int fEnable );
    extern void Abc_Stmap64EnableFinalWitnessScope( Abc_Ntk_t * pNtk, const char * pLabel );

    for ( i = 1; i < argc; i++ )
    {
        if ( !strcmp( argv[i], "-h" ) )
            goto usage;
    }

    printf( "stmap73 diagnostic: inherited-policy = stmap65  accepted-pressure-witness = 1  source = accepted-mode57-pressure-assisted-choices  rank-by = final-criticality,final-slack  mapping-policy-change = 0\n" );
    Map_Stmap73SetAcceptedPressureWitnessDiag( 1 );
    RetValue = Abc_CommandStmap65( pAbc, argc, argv );
    Map_Stmap73SetAcceptedPressureWitnessDiag( 0 );
    if ( RetValue == 0 )
    {
        pNtkRes = Abc_FrameReadNtk( pAbc );
        Abc_Stmap64EnableFinalWitnessScope( pNtkRes, "stmap73" );
    }
    return RetValue;

usage:
    Abc_Print( -2, "usage: stmap73 [-DABFSG float] [-M num] [-arspfuovh]\n" );
    Abc_Print( -2, "\t              performs stmap73 bounded-pressure mapping with accepted pressure-choice witness diagnostics\n" );
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
