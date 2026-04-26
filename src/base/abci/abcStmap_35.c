/**CFile****************************************************************

  FileName    [abcStmap_35.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Versioned AutoEDA standard-cell mapper experiments.]

  Synopsis    [stmap35 command.]

***********************************************************************/

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "map/mio/mio.h"
#include "map/scl/sclLib.h"
#include "misc/extra/extra.h"

ABC_NAMESPACE_IMPL_START

/**Function*************************************************************

  Synopsis    [Inserts one mapped-load diagnostic into the top list.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static void Abc_Stmap35TopInsert( float * pRatios, int * pNodes, const char ** ppGates, int * pFanouts, float * pLoads, float * pCaps, float Ratio, int Node, const char * pGate, int Fanouts, float Load, float Cap )
{
    int i, k;
    if ( Ratio <= pRatios[4] )
        return;
    for ( i = 0; i < 5; i++ )
    {
        if ( Ratio <= pRatios[i] )
            continue;
        for ( k = 4; k > i; k-- )
        {
            pRatios[k] = pRatios[k-1];
            pNodes[k] = pNodes[k-1];
            ppGates[k] = ppGates[k-1];
            pFanouts[k] = pFanouts[k-1];
            pLoads[k] = pLoads[k-1];
            pCaps[k] = pCaps[k-1];
        }
        pRatios[i] = Ratio;
        pNodes[i] = Node;
        ppGates[i] = pGate;
        pFanouts[i] = Fanouts;
        pLoads[i] = Load;
        pCaps[i] = Cap;
        return;
    }
}

/**Function*************************************************************

  Synopsis    [Returns direct SCL input-pin load driven by a mapped node.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static float Abc_Stmap35FanoutLoad( SC_Lib * pLib, Abc_Obj_t * pObj, int * pFanouts, int * pPins )
{
    Abc_Obj_t * pFanout;
    Mio_Gate_t * pGateFan;
    SC_Cell * pCellFan;
    float Load = 0.0;
    int i, iFanin, CellId;
    *pFanouts = 0;
    *pPins = 0;
    Abc_ObjForEachFanout( pObj, pFanout, i )
    {
        (*pFanouts)++;
        if ( !Abc_ObjIsNode(pFanout) || pFanout->pData == NULL )
            continue;
        iFanin = Abc_NodeFindFanin( pFanout, pObj );
        if ( iFanin < 0 )
            continue;
        pGateFan = (Mio_Gate_t *)pFanout->pData;
        CellId = Abc_SclCellFind( pLib, Mio_GateReadName(pGateFan) );
        if ( CellId < 0 )
            continue;
        pCellFan = SC_LibCell( pLib, CellId );
        if ( pCellFan == NULL || iFanin >= pCellFan->n_inputs )
            continue;
        Load += SC_CellPinCap( pCellFan, iFanin );
        (*pPins)++;
    }
    return Load;
}

/**Function*************************************************************

  Synopsis    [Prints direct SCL load diagnostics for the mapped network.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static void Abc_Stmap35PrintSclLoadStats( Abc_Ntk_t * pNtk )
{
    SC_Lib * pLib = (SC_Lib *)Abc_FrameReadLibScl();
    Abc_Obj_t * pObj;
    Mio_Gate_t * pGate;
    SC_Cell * pCell;
    float TopRatios[5] = {0}, TopLoads[5] = {0}, TopCaps[5] = {0};
    const char * pTopGates[5] = {NULL};
    int TopNodes[5] = {0}, TopFanouts[5] = {0};
    float Load, MaxCap, Ratio, RatioSum = 0.0, RatioMax = 0.0;
    int i, CellId, Fanouts, Pins, nNodes = 0, nMatched = 0, nPins = 0, nOver = 0;
    if ( pLib == NULL || !Abc_SclHasDelayInfo(pLib) )
    {
        printf( "stmap35 scl-load stats: unavailable = 1\n" );
        return;
    }
    Abc_NtkForEachNode( pNtk, pObj, i )
    {
        nNodes++;
        if ( pObj->pData == NULL )
            continue;
        pGate = (Mio_Gate_t *)pObj->pData;
        CellId = Abc_SclCellFind( pLib, Mio_GateReadName(pGate) );
        if ( CellId < 0 )
            continue;
        pCell = SC_LibCell( pLib, CellId );
        if ( pCell == NULL || pCell->n_outputs == 0 )
            continue;
        MaxCap = SC_CellPin( pCell, pCell->n_inputs )->max_out_cap;
        if ( MaxCap <= 0.0 )
            continue;
        Load = Abc_Stmap35FanoutLoad( pLib, pObj, &Fanouts, &Pins );
        Ratio = Load / MaxCap;
        RatioSum += Ratio;
        if ( RatioMax < Ratio )
            RatioMax = Ratio;
        if ( Ratio > 1.0 )
            nOver++;
        nMatched++;
        nPins += Pins;
        Abc_Stmap35TopInsert( TopRatios, TopNodes, pTopGates, TopFanouts, TopLoads, TopCaps,
            Ratio, Abc_ObjId(pObj), Mio_GateReadName(pGate), Fanouts, Load, MaxCap );
    }
    printf( "stmap35 scl-load stats: nodes = %d  matched = %d  fanout-pins = %d  over-max = %d  avg-load-ratio = %.3f  max-load-ratio = %.3f\n",
        nNodes, nMatched, nPins, nOver, nMatched ? RatioSum / (float)nMatched : 0.0, RatioMax );
    for ( i = 0; i < 5 && TopRatios[i] > 0.0; i++ )
        printf( "stmap35 scl-load top: rank = %d  node = %d  gate = %s  fanouts = %d  load = %.3f  max-cap = %.3f  load-ratio = %.3f\n",
            i + 1, TopNodes[i], pTopGates[i] ? pTopGates[i] : "?", TopFanouts[i], TopLoads[i], TopCaps[i], TopRatios[i] );
}

/**Function*************************************************************

  Synopsis    [Runs the stmap35 mapper probe.]

  Description [stmap35 keeps the stmap34 bounded mapper penalty and prints
  direct SCL load/max-cap diagnostics for the mapped network.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CommandStmap35( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    Abc_Ntk_t * pNtk, * pNtkRes;
    char Buffer[100];
    double DelayTarget;
    double AreaMulti;
    double DelayMulti;
    float LogFan = 0;
    float Slew = 0; // choose based on the library
    float Gain = 250; // keep classic map gain to isolate the SCL load diagnostic
    int nGatesMin = 0;
    int fAreaOnly;
    int fRecovery;
    int fSweep;
    int fSwitching;
    int fSkipFanout;
    int fUseProfile;
    int fUseBuffs;
    int fVerbose;
    int c;
    extern Abc_Ntk_t * Abc_NtkMap( Abc_Ntk_t * pNtk, Mio_Library_t* userLib, double DelayTarget, double AreaMulti, double DelayMulti, float LogFan, float Slew, float Gain, int nGatesMin, int fRecovery, int fSwitching, int fSkipFanout, int fUseProfile, int fUseBuffs, int fVerbose );
    extern int Abc_NtkFraigSweep( Abc_Ntk_t * pNtk, int fUseInv, int fExdc, int fVerbose, int fVeryVerbose );

    pNtk = Abc_FrameReadNtk(pAbc);
    DelayTarget = -1;
    AreaMulti   = 0;
    DelayMulti  = 0;
    fAreaOnly   = 0;
    fRecovery   = 1;
    fSweep      = 0;
    fSwitching  = 0;
    fSkipFanout = 36; // direct SCL mapped-load diagnostic over the stmap34 penalty; -f disables it
    fUseProfile = 0;
    fUseBuffs   = 0;
    fVerbose    = 0;
    Extra_UtilGetoptReset();
    while ( ( c = Extra_UtilGetopt( argc, argv, "DABFSGMarspfuovh" ) ) != EOF )
    {
        switch ( c )
        {
        case 'D':
            if ( globalUtilOptind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-D\" should be followed by a floating point number.\n" );
                goto usage;
            }
            DelayTarget = (float)atof(argv[globalUtilOptind]);
            globalUtilOptind++;
            if ( DelayTarget <= 0.0 )
                goto usage;
            break;
        case 'A':
            if ( globalUtilOptind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-A\" should be followed by a floating point number.\n" );
                goto usage;
            }
            AreaMulti = (float)atof(argv[globalUtilOptind]);
            globalUtilOptind++;
            break;
        case 'B':
            if ( globalUtilOptind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-B\" should be followed by a floating point number.\n" );
                goto usage;
            }
            DelayMulti = (float)atof(argv[globalUtilOptind]);
            globalUtilOptind++;
            break;
        case 'F':
            if ( globalUtilOptind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-F\" should be followed by a floating point number.\n" );
                goto usage;
            }
            LogFan = (float)atof(argv[globalUtilOptind]);
            globalUtilOptind++;
            if ( LogFan < 0.0 )
                goto usage;
            break;
        case 'S':
            if ( globalUtilOptind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-S\" should be followed by a floating point number.\n" );
                goto usage;
            }
            Slew = (float)atof(argv[globalUtilOptind]);
            globalUtilOptind++;
            if ( Slew <= 0.0 )
                goto usage;
            break;
        case 'G':
            if ( globalUtilOptind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-G\" should be followed by a floating point number.\n" );
                goto usage;
            }
            Gain = (float)atof(argv[globalUtilOptind]);
            globalUtilOptind++;
            if ( Gain <= 0.0 )
                goto usage;
            break;
        case 'M':
            if ( globalUtilOptind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-M\" should be followed by a positive integer.\n" );
                goto usage;
            }
            nGatesMin = atoi(argv[globalUtilOptind]);
            globalUtilOptind++;
            if ( nGatesMin < 0 )
                goto usage;
            break;
        case 'a':
            fAreaOnly ^= 1;
            break;
        case 'r':
            fRecovery ^= 1;
            break;
        case 's':
            fSweep ^= 1;
            break;
        case 'p':
            fSwitching ^= 1;
            break;
        case 'f':
            fSkipFanout = fSkipFanout ? 0 : 36;
            break;
        case 'u':
            fUseProfile ^= 1;
            break;
        case 'o':
            fUseBuffs ^= 1;
            break;
        case 'v':
            fVerbose ^= 1;
            break;
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }

    if ( pNtk == NULL )
    {
        Abc_Print( -1, "Empty network.\n" );
        return 1;
    }

    if ( fAreaOnly )
        DelayTarget = ABC_INFINITY;

    if ( !Abc_NtkIsStrash(pNtk) )
    {
        pNtk = Abc_NtkStrash( pNtk, 0, 0, 0 );
        if ( pNtk == NULL )
        {
            Abc_Print( -1, "Strashing before stmap35 has failed.\n" );
            return 1;
        }
        pNtk = Abc_NtkBalance( pNtkRes = pNtk, 0, 0, 1 );
        Abc_NtkDelete( pNtkRes );
        if ( pNtk == NULL )
        {
            Abc_Print( -1, "Balancing before stmap35 has failed.\n" );
            return 1;
        }
        Abc_Print( 0, "The network was strashed and balanced before stmap35.\n" );
        pNtkRes = Abc_NtkMap( pNtk, NULL, DelayTarget, AreaMulti, DelayMulti, LogFan, Slew, Gain, nGatesMin, fRecovery, fSwitching, fSkipFanout, fUseProfile, fUseBuffs, fVerbose );
        if ( pNtkRes == NULL )
        {
            Abc_NtkDelete( pNtk );
            Abc_Print( -1, "stmap35 has failed.\n" );
            return 1;
        }
        Abc_NtkDelete( pNtk );
    }
    else
    {
        pNtkRes = Abc_NtkMap( pNtk, NULL, DelayTarget, AreaMulti, DelayMulti, LogFan, Slew, Gain, nGatesMin, fRecovery, fSwitching, fSkipFanout, fUseProfile, fUseBuffs, fVerbose );
        if ( pNtkRes == NULL )
        {
            Abc_Print( -1, "stmap35 has failed.\n" );
            return 1;
        }
    }

    if ( fSweep )
    {
        Abc_NtkFraigSweep( pNtkRes, 0, 0, 0, 0 );
        if ( Abc_NtkHasMapping(pNtkRes) )
        {
            pNtkRes = Abc_NtkDupDfs( pNtk = pNtkRes );
            Abc_NtkDelete( pNtk );
        }
    }

    Abc_Stmap35PrintSclLoadStats( pNtkRes );
    Abc_FrameReplaceCurrentNetwork( pAbc, pNtkRes );
    return 0;

usage:
    if ( DelayTarget == -1 )
        sprintf(Buffer, "not used" );
    else
        sprintf(Buffer, "%.3f", DelayTarget );
    Abc_Print( -2, "usage: stmap35 [-DABFSG float] [-M num] [-arspfuovh]\n" );
    Abc_Print( -2, "\t              performs stmap35 standard-cell mapping with direct SCL mapped-load diagnostics\n" );
    Abc_Print( -2, "\t-D float : sets the global required times [default = %s]\n", Buffer );
    Abc_Print( -2, "\t-A float : \"area multiplier\" to bias gate selection [default = %.2f]\n", AreaMulti );
    Abc_Print( -2, "\t-B float : \"delay multiplier\" to bias gate selection [default = %.2f]\n", DelayMulti );
    Abc_Print( -2, "\t-F float : the logarithmic fanout delay parameter [default = %.2f]\n", LogFan );
    Abc_Print( -2, "\t-S float : the slew parameter used to generate the library [default = %.2f]\n", Slew );
    Abc_Print( -2, "\t-G float : the gain parameter used to generate the library [default = %.2f]\n", Gain );
    Abc_Print( -2, "\t-M num   : skip gate classes whose size is less than this [default = %d]\n", nGatesMin );
    Abc_Print( -2, "\t-a       : toggles area-only mapping [default = %s]\n", fAreaOnly? "yes": "no" );
    Abc_Print( -2, "\t-r       : toggles area recovery [default = %s]\n", fRecovery? "yes": "no" );
    Abc_Print( -2, "\t-s       : toggles sweep after mapping [default = %s]\n", fSweep? "yes": "no" );
    Abc_Print( -2, "\t-p       : optimizes power by minimizing switching [default = %s]\n", fSwitching? "yes": "no" );
    Abc_Print( -2, "\t-f       : disables the direct SCL load diagnostic mapper mode [default = %s]\n", fSkipFanout? "yes": "no" );
    Abc_Print( -2, "\t-u       : use standard-cell profile [default = %s]\n", fUseProfile? "yes": "no" );
    Abc_Print( -2, "\t-o       : toggles using buffers to decouple combinational outputs [default = %s]\n", fUseBuffs? "yes": "no" );
    Abc_Print( -2, "\t-v       : toggles verbose output [default = %s]\n", fVerbose? "yes": "no" );
    Abc_Print( -2, "\t-h       : print the command usage\n");
    return 1;
}

ABC_NAMESPACE_IMPL_END
