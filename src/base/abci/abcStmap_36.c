/**CFile****************************************************************

  FileName    [abcStmap_36.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Versioned AutoEDA standard-cell mapper experiments.]

  Synopsis    [stmap36 command.]

***********************************************************************/

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "map/mio/mio.h"
#include "map/scl/sclLib.h"
#include "misc/extra/extra.h"
#include <string.h>

ABC_NAMESPACE_IMPL_START

typedef struct Abc_Stmap36LoadStats_t_
{
    int fAvailable;
    int nNodes;
    int nMatched;
    int nPins;
    int nOver;
    float RatioSum;
    float RatioMax;
    float TopRatios[5];
    float TopLoads[5];
    float TopCaps[5];
    const char * pTopGates[5];
    int TopNodes[5];
    int TopFanouts[5];
} Abc_Stmap36LoadStats_t;

static void Abc_Stmap36LoadStatsClear( Abc_Stmap36LoadStats_t * pStats )
{
    memset( pStats, 0, sizeof(Abc_Stmap36LoadStats_t) );
}

/**Function*************************************************************

  Synopsis    [Inserts one mapped-load diagnostic into the top list.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static void Abc_Stmap36TopInsert( float * pRatios, int * pNodes, const char ** ppGates, int * pFanouts, float * pLoads, float * pCaps, float Ratio, int Node, const char * pGate, int Fanouts, float Load, float Cap )
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
static float Abc_Stmap36FanoutLoad( SC_Lib * pLib, Abc_Obj_t * pObj, int * pFanouts, int * pPins )
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
static void Abc_Stmap36CollectSclLoadStats( Abc_Ntk_t * pNtk, Abc_Stmap36LoadStats_t * pStats )
{
    SC_Lib * pLib = (SC_Lib *)Abc_FrameReadLibScl();
    Abc_Obj_t * pObj;
    Mio_Gate_t * pGate;
    SC_Cell * pCell;
    float Load, MaxCap, Ratio;
    int i, CellId, Fanouts, Pins;
    Abc_Stmap36LoadStatsClear( pStats );
    if ( pLib == NULL || !Abc_SclHasDelayInfo(pLib) )
        return;
    pStats->fAvailable = 1;
    Abc_NtkForEachNode( pNtk, pObj, i )
    {
        pStats->nNodes++;
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
        Load = Abc_Stmap36FanoutLoad( pLib, pObj, &Fanouts, &Pins );
        Ratio = Load / MaxCap;
        pStats->RatioSum += Ratio;
        if ( pStats->RatioMax < Ratio )
            pStats->RatioMax = Ratio;
        if ( Ratio > 1.0 )
            pStats->nOver++;
        pStats->nMatched++;
        pStats->nPins += Pins;
        Abc_Stmap36TopInsert( pStats->TopRatios, pStats->TopNodes, pStats->pTopGates, pStats->TopFanouts, pStats->TopLoads, pStats->TopCaps,
            Ratio, Abc_ObjId(pObj), Mio_GateReadName(pGate), Fanouts, Load, MaxCap );
    }
}

static void Abc_Stmap36PrintSclLoadStats( const char * pLabel, Abc_Stmap36LoadStats_t * pStats )
{
    int i;
    if ( !pStats->fAvailable )
    {
        printf( "stmap36 %s stats: unavailable = 1\n", pLabel );
        return;
    }
    printf( "stmap36 %s stats: nodes = %d  matched = %d  fanout-pins = %d  over-max = %d  avg-load-ratio = %.3f  max-load-ratio = %.3f\n",
        pLabel, pStats->nNodes, pStats->nMatched, pStats->nPins, pStats->nOver,
        pStats->nMatched ? pStats->RatioSum / (float)pStats->nMatched : 0.0, pStats->RatioMax );
    for ( i = 0; i < 5 && pStats->TopRatios[i] > 0.0; i++ )
        printf( "stmap36 %s top: rank = %d  node = %d  gate = %s  fanouts = %d  load = %.3f  max-cap = %.3f  load-ratio = %.3f\n",
            pLabel, i + 1, pStats->TopNodes[i], pStats->pTopGates[i] ? pStats->pTopGates[i] : "?",
            pStats->TopFanouts[i], pStats->TopLoads[i], pStats->TopCaps[i], pStats->TopRatios[i] );
}

static float Abc_Stmap36FeedbackSeverity( Abc_Stmap36LoadStats_t * pStats )
{
    float OverFrac, MaxPart, OverPart, Severity;
    if ( !pStats->fAvailable || pStats->nMatched == 0 )
        return 0.0;
    OverFrac = (float)pStats->nOver / (float)pStats->nMatched;
    MaxPart = pStats->RatioMax > 4.0 ? (pStats->RatioMax - 4.0) / 8.0 : 0.0;
    OverPart = OverFrac > 0.002 ? (OverFrac - 0.002) / 0.006 : 0.0;
    if ( MaxPart > 1.0 )
        MaxPart = 1.0;
    if ( OverPart > 1.0 )
        OverPart = 1.0;
    Severity = 0.65 * MaxPart + 0.35 * OverPart;
    return Severity > 1.0 ? 1.0 : Severity;
}

/**Function*************************************************************

  Synopsis    [Runs the stmap36 mapper probe.]

  Description [stmap36 measures direct SCL load/max-cap pressure after an
  initial map and feeds the aggregate severity into a second mapper pass.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CommandStmap36( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    Abc_Ntk_t * pNtk, * pNtkMap, * pNtkRes, * pNtkFirst;
    Abc_Stmap36LoadStats_t LoadStats;
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
    int c, fTempMap = 0;
    float OverFrac, Feedback;
    extern Abc_Ntk_t * Abc_NtkMap( Abc_Ntk_t * pNtk, Mio_Library_t* userLib, double DelayTarget, double AreaMulti, double DelayMulti, float LogFan, float Slew, float Gain, int nGatesMin, int fRecovery, int fSwitching, int fSkipFanout, int fUseProfile, int fUseBuffs, int fVerbose );
    extern int Abc_NtkFraigSweep( Abc_Ntk_t * pNtk, int fUseInv, int fExdc, int fVerbose, int fVeryVerbose );
    extern void Map_Stmap36SetSclLoadFeedback( float MaxLoadRatio, float OverFrac, float Severity );

    pNtk = Abc_FrameReadNtk(pAbc);
    DelayTarget = -1;
    AreaMulti   = 0;
    DelayMulti  = 0;
    fAreaOnly   = 0;
    fRecovery   = 1;
    fSweep      = 0;
    fSwitching  = 0;
    fSkipFanout = 37; // SCL feedback pass over the stmap35 mapper surface; -f disables it
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
            fSkipFanout = fSkipFanout ? 0 : 37;
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

    Map_Stmap36SetSclLoadFeedback( 0.0, 0.0, 0.0 );
    pNtkMap = pNtk;
    if ( !Abc_NtkIsStrash(pNtk) )
    {
        pNtkMap = Abc_NtkStrash( pNtk, 0, 0, 0 );
        if ( pNtkMap == NULL )
        {
            Abc_Print( -1, "Strashing before stmap36 has failed.\n" );
            return 1;
        }
        pNtkMap = Abc_NtkBalance( pNtkRes = pNtkMap, 0, 0, 1 );
        Abc_NtkDelete( pNtkRes );
        if ( pNtkMap == NULL )
        {
            Abc_Print( -1, "Balancing before stmap36 has failed.\n" );
            return 1;
        }
        fTempMap = 1;
        Abc_Print( 0, "The network was strashed and balanced before stmap36.\n" );
    }

    if ( fSkipFanout )
    {
        pNtkFirst = Abc_NtkMap( pNtkMap, NULL, DelayTarget, AreaMulti, DelayMulti, LogFan, Slew, Gain, nGatesMin, fRecovery, fSwitching, 36, fUseProfile, fUseBuffs, fVerbose );
        if ( pNtkFirst == NULL )
        {
            if ( fTempMap )
                Abc_NtkDelete( pNtkMap );
            Abc_Print( -1, "stmap36 has failed.\n" );
            return 1;
        }
        Abc_Stmap36CollectSclLoadStats( pNtkFirst, &LoadStats );
        Abc_Stmap36PrintSclLoadStats( "feedback-load", &LoadStats );
        OverFrac = LoadStats.nMatched ? (float)LoadStats.nOver / (float)LoadStats.nMatched : 0.0;
        Feedback = Abc_Stmap36FeedbackSeverity( &LoadStats );
        printf( "stmap36 feedback: remap = 1  active = %d  max-load-ratio = %.3f  over-max-frac = %.6f  severity = %.3f\n",
            Feedback > 0.0, LoadStats.RatioMax, OverFrac, Feedback );
        Abc_NtkDelete( pNtkFirst );
        Map_Stmap36SetSclLoadFeedback( LoadStats.RatioMax, OverFrac, Feedback );
        pNtkRes = Abc_NtkMap( pNtkMap, NULL, DelayTarget, AreaMulti, DelayMulti, LogFan, Slew, Gain, nGatesMin, fRecovery, fSwitching, 37, fUseProfile, fUseBuffs, fVerbose );
        Map_Stmap36SetSclLoadFeedback( 0.0, 0.0, 0.0 );
        if ( pNtkRes == NULL )
        {
            if ( fTempMap )
                Abc_NtkDelete( pNtkMap );
            Abc_Print( -1, "stmap36 feedback remap has failed.\n" );
            return 1;
        }
    }
    else
    {
        pNtkRes = Abc_NtkMap( pNtkMap, NULL, DelayTarget, AreaMulti, DelayMulti, LogFan, Slew, Gain, nGatesMin, fRecovery, fSwitching, 0, fUseProfile, fUseBuffs, fVerbose );
        if ( pNtkRes == NULL )
        {
            if ( fTempMap )
                Abc_NtkDelete( pNtkMap );
            Abc_Print( -1, "stmap36 has failed.\n" );
            return 1;
        }
    }
    if ( fTempMap )
        Abc_NtkDelete( pNtkMap );

    if ( fSweep )
    {
        Abc_NtkFraigSweep( pNtkRes, 0, 0, 0, 0 );
        if ( Abc_NtkHasMapping(pNtkRes) )
        {
            pNtkRes = Abc_NtkDupDfs( pNtk = pNtkRes );
            Abc_NtkDelete( pNtk );
        }
    }

    Abc_Stmap36CollectSclLoadStats( pNtkRes, &LoadStats );
    Abc_Stmap36PrintSclLoadStats( "scl-load", &LoadStats );
    Abc_FrameReplaceCurrentNetwork( pAbc, pNtkRes );
    return 0;

usage:
    if ( DelayTarget == -1 )
        sprintf(Buffer, "not used" );
    else
        sprintf(Buffer, "%.3f", DelayTarget );
    Abc_Print( -2, "usage: stmap36 [-DABFSG float] [-M num] [-arspfuovh]\n" );
    Abc_Print( -2, "\t              performs stmap36 two-pass standard-cell mapping with SCL load feedback\n" );
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
    Abc_Print( -2, "\t-f       : disables the SCL load feedback mapper mode [default = %s]\n", fSkipFanout? "yes": "no" );
    Abc_Print( -2, "\t-u       : use standard-cell profile [default = %s]\n", fUseProfile? "yes": "no" );
    Abc_Print( -2, "\t-o       : toggles using buffers to decouple combinational outputs [default = %s]\n", fUseBuffs? "yes": "no" );
    Abc_Print( -2, "\t-v       : toggles verbose output [default = %s]\n", fVerbose? "yes": "no" );
    Abc_Print( -2, "\t-h       : print the command usage\n");
    return 1;
}

ABC_NAMESPACE_IMPL_END
