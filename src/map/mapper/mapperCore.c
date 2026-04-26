/**CFile****************************************************************

  FileName    [mapperCore.c]

  PackageName [MVSIS 1.3: Multi-valued logic synthesis system.]

  Synopsis    [Generic technology mapping engine.]

  Author      [MVSIS Group]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 2.0. Started - June 1, 2004.]

  Revision    [$Id: mapperCore.c,v 1.7 2004/10/01 23:41:04 satrajit Exp $]

***********************************************************************/

#include "mapperInt.h"
//#include "resm.h"

ABC_NAMESPACE_IMPL_START


////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Performs technology mapping for the given object graph.]

  Description [The object graph is stored in the mapping manager.
  First, the AND nodes that fanout into POs are collected in the DFS order.
  Two preprocessing steps are performed: the k-feasible cuts are computed 
  for each node and the truth tables are computed for each cut. Next, the 
  delay-optimal matches are assigned for each node, followed by several 
  iterations of area recoveryd: using area flow (global optimization) 
  and using exact area at a node (local optimization).]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Map_Mapping( Map_Man_t * p )
{
    int fShowSwitching         = 0;
    int fUseAreaFlow           = 1;
    int fUseExactArea          = !p->fSwitching;
    int fUseExactAreaWithPhase = !p->fSwitching;
    abctime clk;

    //////////////////////////////////////////////////////////////////////
    // perform pre-mapping computations
    if ( p->fVerbose )
        Map_MappingReportChoices( p ); 
    Map_MappingSetChoiceLevels( p ); // should always be called before mapping!
//    return 1;

    // compute the cuts of nodes in the DFS order
    clk = Abc_Clock();
    Map_MappingCuts( p );
    p->timeCuts = Abc_Clock() - clk;
    // derive the truth tables 
    clk = Abc_Clock();
    Map_MappingTruths( p );
    p->timeTruth = Abc_Clock() - clk;
    //////////////////////////////////////////////////////////////////////
//ABC_PRT( "Truths", Abc_Clock() - clk );

    //////////////////////////////////////////////////////////////////////
    // compute the minimum-delay mapping
    clk = Abc_Clock();
    p->fMappingMode = 0;
    if ( !Map_MappingMatches( p ) )
        return 0;
    p->timeMatch = Abc_Clock() - clk;
    // compute the references and collect the nodes used in the mapping
    Map_MappingSetRefs( p );
    p->AreaBase = Map_MappingGetArea( p );
if ( p->fVerbose )
{
printf( "Delay    : %s = %8.2f  Flow = %11.1f  Area = %11.1f  %4.1f %%   ", 
                    fShowSwitching? "Switch" : "Delay", 
                    fShowSwitching? Map_MappingGetSwitching(p) : p->fRequiredGlo, 
                    Map_MappingGetAreaFlow(p), p->AreaBase, 0.0 );
ABC_PRT( "Time", p->timeMatch );
}
    //////////////////////////////////////////////////////////////////////

    if ( !p->fAreaRecovery )
    {
        if ( p->fVerbose )
            Map_MappingPrintOutputArrivals( p );
        return 1;
    }

    //////////////////////////////////////////////////////////////////////
    // perform area recovery using area flow
    clk = Abc_Clock();
    if ( fUseAreaFlow )
    {
        // compute the required times
        Map_TimeComputeRequiredGlobal( p );
        // recover area flow
        p->fMappingMode = 1;
        Map_MappingMatches( p );
        // compute the references and collect the nodes used in the mapping
        Map_MappingSetRefs( p );
        p->AreaFinal = Map_MappingGetArea( p );
if ( p->fVerbose )
{
printf( "AreaFlow : %s = %8.2f  Flow = %11.1f  Area = %11.1f  %4.1f %%   ", 
                    fShowSwitching? "Switch" : "Delay", 
                    fShowSwitching? Map_MappingGetSwitching(p) : p->fRequiredGlo, 
                    Map_MappingGetAreaFlow(p), p->AreaFinal, 
                    100.0*(p->AreaBase-p->AreaFinal)/p->AreaBase );
ABC_PRT( "Time", Abc_Clock() - clk );
}
    }
    p->timeArea += Abc_Clock() - clk;
    //////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////
    // perform area recovery using exact area
    clk = Abc_Clock();
    if ( fUseExactArea )
    {
        // compute the required times
        Map_TimeComputeRequiredGlobal( p );
        // recover area
        p->fMappingMode = 2;
        Map_MappingMatches( p );
        // compute the references and collect the nodes used in the mapping
        Map_MappingSetRefs( p );
        p->AreaFinal = Map_MappingGetArea( p );
if ( p->fVerbose )
{
printf( "Area     : %s = %8.2f  Flow = %11.1f  Area = %11.1f  %4.1f %%   ", 
                    fShowSwitching? "Switch" : "Delay", 
                    fShowSwitching? Map_MappingGetSwitching(p) : p->fRequiredGlo, 
                    0.0, p->AreaFinal, 
                    100.0*(p->AreaBase-p->AreaFinal)/p->AreaBase );
ABC_PRT( "Time", Abc_Clock() - clk );
}
    }
    p->timeArea += Abc_Clock() - clk;
    //////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////
    // perform area recovery using exact area
    clk = Abc_Clock();
    if ( fUseExactAreaWithPhase )
    {
        // compute the required times
        Map_TimeComputeRequiredGlobal( p );
        // recover area
        p->fMappingMode = 3;
        Map_MappingMatches( p );
        // compute the references and collect the nodes used in the mapping
        Map_MappingSetRefs( p );
        p->AreaFinal = Map_MappingGetArea( p );
if ( p->fVerbose )
{
printf( "Area     : %s = %8.2f  Flow = %11.1f  Area = %11.1f  %4.1f %%   ", 
                    fShowSwitching? "Switch" : "Delay", 
                    fShowSwitching? Map_MappingGetSwitching(p) : p->fRequiredGlo, 
                    0.0, p->AreaFinal, 
                    100.0*(p->AreaBase-p->AreaFinal)/p->AreaBase );
ABC_PRT( "Time", Abc_Clock() - clk );
}
    }
    p->timeArea += Abc_Clock() - clk;
    //////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////
    // perform area recovery using exact area
    clk = Abc_Clock();
    if ( p->fSwitching )
    {
        // compute the required times
        Map_TimeComputeRequiredGlobal( p );
        // recover switching activity
        p->fMappingMode = 4;
        Map_MappingMatches( p );
        // compute the references and collect the nodes used in the mapping
        Map_MappingSetRefs( p );
        p->AreaFinal = Map_MappingGetArea( p );
if ( p->fVerbose )
{
printf( "Switching: %s = %8.2f  Flow = %11.1f  Area = %11.1f  %4.1f %%   ", 
                    fShowSwitching? "Switch" : "Delay", 
                    fShowSwitching? Map_MappingGetSwitching(p) : p->fRequiredGlo, 
                    0.0, p->AreaFinal, 
                    100.0*(p->AreaBase-p->AreaFinal)/p->AreaBase );
ABC_PRT( "Time", Abc_Clock() - clk );
}

        // compute the required times
        Map_TimeComputeRequiredGlobal( p );
        // recover switching activity
        p->fMappingMode = 4;
        Map_MappingMatches( p );
        // compute the references and collect the nodes used in the mapping
        Map_MappingSetRefs( p );
        p->AreaFinal = Map_MappingGetArea( p );
if ( p->fVerbose )
{
printf( "Switching: %s = %8.2f  Flow = %11.1f  Area = %11.1f  %4.1f %%   ", 
                    fShowSwitching? "Switch" : "Delay", 
                    fShowSwitching? Map_MappingGetSwitching(p) : p->fRequiredGlo, 
                    0.0, p->AreaFinal, 
                    100.0*(p->AreaBase-p->AreaFinal)/p->AreaBase );
ABC_PRT( "Time", Abc_Clock() - clk );
}
    }
    p->timeArea += Abc_Clock() - clk;
    //////////////////////////////////////////////////////////////////////

    if ( p->fSkipFanout >= 14 && p->fSkipFanout <= 41 )
        printf( "stmap%d guard stats: exact-risk = %d  highest = %d  lower-mod = %d  upper-mod = %d  middle-slack = %d  middle-relief = %d  reject-slack = %d  reject-highest = %d  reject-arrival = %d  reject-area = %d\n",
            p->fSkipFanout - 1,
            p->nStmap13ExactRisk, p->nStmap13HighestRisk, p->nStmap13LowerModRisk,
            p->nStmap13UpperModRisk, p->nStmap13MiddleSlack, p->nStmap13MiddleRelief,
            p->nStmap13RejectSlack, p->nStmap13RejectHighest, p->nStmap13RejectArrival,
            p->nStmap13RejectArea );
    if ( p->fSkipFanout >= 19 && p->fSkipFanout <= 41 )
        printf( "stmap%d near-miss stats: near-miss = %d\n", p->fSkipFanout - 1, p->nStmap18NearMiss );
    if ( p->fSkipFanout >= 20 && p->fSkipFanout <= 41 )
        printf( "stmap%d early-seed stats: early-seed = %d\n", p->fSkipFanout - 1, p->nStmap19EarlySeed );
    if ( p->fSkipFanout == 26 )
        printf( "stmap25 ablation stats: moderate-ablation-seed = %d\n", p->nStmap25ModerateAblationSeed );
    if ( p->fSkipFanout == 27 )
        printf( "stmap26 ablation stats: moderate-ablation-seen = %d  moderate-ablation-seed = %d\n", p->nStmap26ModerateAblationSeen, p->nStmap26ModerateAblationSeed );
    if ( p->fSkipFanout == 28 )
        printf( "stmap27 signature stats: moderate-signature-seed = %d  moderate-signature-blocked = %d\n", p->nStmap27ModerateSignatureSeed, p->nStmap27ModerateSignatureBlocked );
    if ( p->fSkipFanout == 29 )
        printf( "stmap28 penalty stats: moderate-penalty-seed = %d  moderate-penalty-blocked = %d\n", p->nStmap28ModeratePenaltySeed, p->nStmap28ModeratePenaltyBlocked );
    if ( p->fSkipFanout == 30 )
        printf( "stmap29 penalty stats: moderate-penalty-seed = %d  moderate-penalty-blocked = %d\n", p->nStmap29ModeratePenaltySeed, p->nStmap29ModeratePenaltyBlocked );
    if ( p->fSkipFanout == 31 )
        printf( "stmap30 penalty stats: moderate-penalty-seed = %d  moderate-penalty-blocked = %d  strong-penalty-seed = %d  strong-penalty-blocked = %d\n",
            p->nStmap30ModeratePenaltySeed, p->nStmap30ModeratePenaltyBlocked,
            p->nStmap30StrongPenaltySeed, p->nStmap30StrongPenaltyBlocked );
    if ( p->fSkipFanout == 32 )
        printf( "stmap31 penalty stats: moderate-penalty-seed = %d  moderate-penalty-blocked = %d  strong-penalty-seed = %d  strong-penalty-blocked = %d\n",
            p->nStmap31ModeratePenaltySeed, p->nStmap31ModeratePenaltyBlocked,
            p->nStmap31StrongPenaltySeed, p->nStmap31StrongPenaltyBlocked );
    if ( p->fSkipFanout == 33 )
        printf( "stmap32 penalty stats: moderate-penalty-seed = %d  moderate-penalty-blocked = %d  strong-penalty-seed = %d  strong-penalty-blocked = %d\n",
            p->nStmap32ModeratePenaltySeed, p->nStmap32ModeratePenaltyBlocked,
            p->nStmap32StrongPenaltySeed, p->nStmap32StrongPenaltyBlocked );
    if ( p->fSkipFanout == 34 )
        printf( "stmap33 penalty stats: moderate-penalty-seed = %d  moderate-penalty-blocked = %d  strong-penalty-seed = %d  strong-penalty-blocked = %d\n",
            p->nStmap33ModeratePenaltySeed, p->nStmap33ModeratePenaltyBlocked,
            p->nStmap33StrongPenaltySeed, p->nStmap33StrongPenaltyBlocked );
    if ( p->fSkipFanout == 35 )
        printf( "stmap34 penalty stats: moderate-penalty-seed = %d  moderate-penalty-blocked = %d  strong-penalty-seed = %d  strong-penalty-blocked = %d\n",
            p->nStmap34ModeratePenaltySeed, p->nStmap34ModeratePenaltyBlocked,
            p->nStmap34StrongPenaltySeed, p->nStmap34StrongPenaltyBlocked );
    if ( p->fSkipFanout == 36 )
        printf( "stmap35 penalty stats: moderate-penalty-seed = %d  moderate-penalty-blocked = %d  strong-penalty-seed = %d  strong-penalty-blocked = %d\n",
            p->nStmap35ModeratePenaltySeed, p->nStmap35ModeratePenaltyBlocked,
            p->nStmap35StrongPenaltySeed, p->nStmap35StrongPenaltyBlocked );
    if ( p->fSkipFanout == 37 )
        printf( "stmap36 penalty stats: moderate-penalty-seed = %d  moderate-penalty-blocked = %d  strong-penalty-seed = %d  strong-penalty-blocked = %d\n",
            p->nStmap36ModeratePenaltySeed, p->nStmap36ModeratePenaltyBlocked,
            p->nStmap36StrongPenaltySeed, p->nStmap36StrongPenaltyBlocked );
    if ( p->fSkipFanout == 38 )
        printf( "stmap37 penalty stats: moderate-penalty-seed = %d  moderate-penalty-blocked = %d  strong-penalty-seed = %d  strong-penalty-blocked = %d\n",
            p->nStmap37ModeratePenaltySeed, p->nStmap37ModeratePenaltyBlocked,
            p->nStmap37StrongPenaltySeed, p->nStmap37StrongPenaltyBlocked );
    if ( p->fSkipFanout == 39 )
        printf( "stmap38 penalty stats: moderate-penalty-seed = %d  moderate-penalty-blocked = %d  strong-penalty-seed = %d  strong-penalty-blocked = %d\n",
            p->nStmap38ModeratePenaltySeed, p->nStmap38ModeratePenaltyBlocked,
            p->nStmap38StrongPenaltySeed, p->nStmap38StrongPenaltyBlocked );
    if ( p->fSkipFanout == 40 )
        printf( "stmap39 penalty stats: moderate-penalty-seed = %d  moderate-penalty-blocked = %d  strong-penalty-seed = %d  strong-penalty-blocked = %d\n",
            p->nStmap39ModeratePenaltySeed, p->nStmap39ModeratePenaltyBlocked,
            p->nStmap39StrongPenaltySeed, p->nStmap39StrongPenaltyBlocked );
    if ( p->fSkipFanout == 41 )
        printf( "stmap40 penalty stats: moderate-penalty-seed = %d  moderate-penalty-blocked = %d  strong-penalty-seed = %d  strong-penalty-blocked = %d\n",
            p->nStmap40ModeratePenaltySeed, p->nStmap40ModeratePenaltyBlocked,
            p->nStmap40StrongPenaltySeed, p->nStmap40StrongPenaltyBlocked );

    // print the arrival times of the latest outputs
    if ( p->fVerbose )
        Map_MappingPrintOutputArrivals( p );
    return 1;
}
ABC_NAMESPACE_IMPL_END
