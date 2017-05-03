/*
 *       {_       {__       {__{_______              {__      {__
 *      {_ __     {_ {__   {___{__    {__             {__   {__  
 *     {_  {__    {__ {__ { {__{__    {__     {__      {__ {__   
 *    {__   {__   {__  {__  {__{_ {__       {_   {__     {__     
 *   {______ {__  {__   {_  {__{__  {__    {_____ {__  {__ {__   
 *  {__       {__ {__       {__{__    {__  {_         {__   {__  
 * {__         {__{__       {__{__      {__  {____   {__      {__
 *
 */

#include "AMReX_EBArith.H"
#include "AMReX_EBFastFR.H"
#include "AMReX_EBFluxFactory.H"

namespace amrex
{
/*******************/
  EBFastFR::
  EBFastFR(const EBLevelGrid& a_eblgFine,
           const EBLevelGrid& a_eblgCoar,
           const int&         a_refRat,
           const int&         a_nvar,
           bool a_forceNoEBCF)
  {
    define(a_eblgFine, a_eblgCoar, a_refRat, a_nvar, a_forceNoEBCF);
  }
/*******************/
  void
  EBFastFR::
  define(const EBLevelGrid& a_eblgFine,
         const EBLevelGrid& a_eblgCoar,
         const int&         a_refRat,
         const int&         a_nvar,
         bool a_forceNoEBCF)
  {
    BL_PROFILE("EBFastFR::define");

    m_refRat   = a_refRat;
    m_nComp    = a_nvar;
    m_eblgFine = a_eblgFine;
    m_eblgCoar = a_eblgCoar;
    if (a_forceNoEBCF)
    {
      m_hasEBCF = false;
    }
    else
    {
      amrex::Error("EB/CF bit not ported yet for flux register");
    }
    if (!m_eblgFine.coarsenable(a_refRat))
    {
      amrex::Error("EBFastFR::define: dbl not coarsenable by refrat");
    }

    //need this to store coarse fluxes because FluxRegister is weird.
    EBFluxFactory factCoar(m_eblgCoar.getEBISL());
    EBFluxFactory factFine(m_eblgFine.getEBISL());
    m_coarRegister.define(m_eblgCoar.getDBL(),m_eblgCoar.getDM(), m_nComp, 0, MFInfo(), factCoar);
    m_fineRegister.define(m_eblgFine.getDBL(),m_eblgFine.getDM(), m_nComp, 0, MFInfo(), factFine);

    int fake_lev_num = 1;
    IntVect refRat = m_refRat*IntVect::Unit;
    m_nonEBFluxReg.define(m_eblgFine.getDBL(), m_eblgFine.getDM(), refRat, fake_lev_num, m_nComp);
    //set all registers to zero to start.
    setToZero();
    m_isDefined = true;
  }

/*******************/
  void
  EBFastFR::
  setToZero()
  {
    BL_PROFILE("EBFastFR::setToZero");
    m_nonEBFluxReg.setVal(0.);
    EBLevelDataOps::setVal(m_coarRegister, 0.0);
    EBLevelDataOps::setVal(m_fineRegister, 0.0);
  }
/*******************/
  void
  EBFastFR::
  incrementCoarse(const EBFaceFAB&      a_coarFlux,
                  const Real&           a_scale,
                  const MFIter   &      a_coarMFI,
                  int isrc, int idst, int inco)
  {
    BL_PROFILE("EBFastFR::incrementCoarse");
    int idir = a_coarFlux.direction();
    m_coarRegister[a_coarMFI][idir].plus(a_coarFlux, isrc, idst, inco, a_scale);
  }
  void
  EBFastFR::
  incrementFine(const EBFaceFAB&      a_fineFlux,
                const Real&           a_scale,
                const MFIter&         a_fineMFI,
                int isrc, int idst, int inco)
  {
    BL_PROFILE("EBFastFR::incrementFine");
    int idir = a_fineFlux.direction();
    m_fineRegister[a_fineMFI][idir].plus(a_fineFlux, isrc, idst, inco, a_scale);
  }
/*******************/
  void
  EBFastFR::
  reflux(FabArray<EBCellFAB> & a_uCoar,
         const Real&           a_scale,
         int isrc, int idst, int inco)
  {
    BL_PROFILE("EBFastFR::reflux");
    //boxlib's flux register does not scale finefaces/coarseface for you.
    int numCoarFacesPerFine = D_TERM(1, *m_refRat, *m_refRat);
    //boxlib's flux register also does not do the sign change for you
    Real coarScale = -numCoarFacesPerFine;
    Real fineScale = 1;
    for(int idir = 0; idir < SpaceDim; idir++)
    {
      shared_ptr<MultiFab> regCoarFlux, regFineFlux;
      EBLevelDataOps::aliasIntoMF(regCoarFlux, m_coarRegister, idir, m_eblgCoar);
      EBLevelDataOps::aliasIntoMF(regFineFlux, m_fineRegister, idir, m_eblgFine);
      //scales already added in
      m_nonEBFluxReg.CrseInit(*regCoarFlux, idir, isrc, idst, inco, coarScale);
      m_nonEBFluxReg.FineAdd (*regFineFlux, idir, isrc, idst, inco, fineScale);
    }
    shared_ptr<MultiFab> regCoarSoln; 
    EBLevelDataOps::aliasIntoMF(regCoarSoln, a_uCoar, m_eblgCoar);
    Geometry crse_geom(m_eblgCoar.getDomain());

    m_nonEBFluxReg.ClearInternalBorders(crse_geom);
    m_nonEBFluxReg.Reflux(*regCoarSoln, a_scale, isrc, idst, inco, crse_geom);
  }
}

