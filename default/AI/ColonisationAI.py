#get these declared first to help avoid import circularities
import freeOrionAIInterface as fo # pylint: disable=import-error

import AIDependencies
import AITarget
import AIstate
import FleetUtilsAI
import FreeOrionAI as foAI
import PlanetUtilsAI
import ProductionAI
import TechsListsAI
from EnumsAI import AIFleetMissionType, AIExplorableSystemType, AITargetType, AIFocusType
import EnumsAI
from time import time
import math


empireSpecies = {}
empireSpeciesByPlanet = {}
empireSpeciesSystems={}
empireColonizers = {}
empireShipBuilders={}
empireShipyards = {}
empire_dry_docks = {}
availableGrowthSpecials={}
empirePlanetsWithGrowthSpecials={}
activeGrowthSpecials={}
empireMetabolisms={}
annexableSystemIDs=set()
annexableRing1=set()
annexableRing2=set()
annexableRing3=set()
annexablePlanetIDs=set()
curBestMilShipRating = 20
allColonyOpportunities = {}
gotRuins=False
gotAst = False
gotGG = False
curBestPilotRating = 1e-8
curMidPilotRating = 1e-8
pilotRatings = {}
colony_status = {}
pop_map = {}
empire_status = {'industrialists':0,  'researchers':0} 


environs =                  { str(fo.planetEnvironment.uninhabitable): 0,  str(fo.planetEnvironment.hostile): 1,  str(fo.planetEnvironment.poor): 2,  str(fo.planetEnvironment.adequate): 3,  str(fo.planetEnvironment.good):4 }
photoMap= { fo.starType.blue:3    , fo.starType.white:1.5  , fo.starType.red:-1 ,  fo.starType.neutron: -1 , fo.starType.blackHole: -10 , fo.starType.noStar: -10     }
#   mods per environ    uninhab   hostile    poor   adequate    good
popSizeModMap={
                            "env":               [ 0, -4, -2, 0,  3 ],
                            "subHab":      [ 0,  1,  1,  1,  1 ],
                            "symBio":       [ 0,  0,  1,  1,  1 ],
                            "xenoGen":  [ 0,  1,  2,  2,  0 ],
                            "xenoHyb":   [ 0,  2,  1,  0,  0 ],
                            "cyborg":       [ 0,  2,  0,  0,  0 ],
                            "ndim":            [ 0, 2,  2,  2,   2 ],
                            "orbit":            [ 0,  1,  1,  1,  1 ],
                            "gaia":             [ 0,  3,  3,  3,  3 ],
                            }

nestValMap = {
              "SNOWFLAKE_NEST_SPECIAL": 15,
              "KRAKEN_NEST_SPECIAL":40,
              "JUGGERNAUT_NEST_SPECIAL":80,
              }

def dictFromMap(this_map):
    return dict(  [  (el.key(),  el.data() ) for el in this_map ] )
    
def resetCAIGlobals():
    global curBestMilShipRating
    empireSpecies.clear()
    empireSpeciesByPlanet.clear()
    empireSpeciesSystems.clear()
    empireColonizers.clear()
    empireShipBuilders.clear()
    empireShipyards.clear()
    empire_dry_docks.clear()
    activeGrowthSpecials.clear()
    annexableSystemIDs.clear()
    annexableRing1.clear()
    annexableRing2.clear()
    annexableRing3.clear()
    annexablePlanetIDs.clear()
    curBestMilShipRating = 20
    allColonyOpportunities.clear()

def ratePilotingTag(tagList):
    grade = 2.0
    grades = {'NO':1e-8, 'BAD':0.75, 'GOOD':4.0,  'GREAT':6.0,  'ULTIMATE':12.0 }
    for tag in [tag1 for tag1 in tagList if (("AI_TAG" in tag1) and ("WEAPONS" in tag1)) ]:
        tagParts = tag.split('_')
        tagType = tagParts[3]
        grade = grades.get(tagParts[2],  1.0)
    return grade

def ratePlanetaryPiloting(pid):
    universe = fo.getUniverse()
    planet = universe.getPlanet(pid)
    if (not planet) or (not planet.speciesName):
        return 0.0
    thisSpec = fo.getSpecies(planet.speciesName)
    if not thisSpec:
        return 0.0
    return ratePilotingTag(thisSpec.tags)

def getColonyFleets():
    "examines known planets, collects various colonization data, to be later used to send colony fleets"
    global  curBestMilShipRating,  gotRuins, gotAst,  gotGG,  curBestPilotRating, curMidPilotRating
    times=[]
    tasks = []
    times.append( time() )
    tasks.append("init")

    curBestMilShipRating = ProductionAI.curBestMilShipRating()
    times.append( time() )
    tasks.append( "getting best milship rating" )


    allColonyFleetIDs = FleetUtilsAI.getEmpireFleetIDsByRole(AIFleetMissionType.FLEET_MISSION_COLONISATION)
    AIstate.colonyFleetIDs[:] = FleetUtilsAI.extractFleetIDsWithoutMissionTypes(allColonyFleetIDs)
    times.append( time() )
    tasks.append( "getting avail colony fleets" )

    # get suppliable systems and planets
    universe = fo.getUniverse()
    empire = fo.getEmpire()
    empireID = empire.empireID
    capitalID = PlanetUtilsAI.getCapital()
    #capitalID = empire.capitalID
    homeworld=None
    if capitalID:
        homeworld = universe.getPlanet(capitalID)
    if homeworld:
        speciesName = homeworld.speciesName
        homeworldName=homeworld.name
        homeSystemID = homeworld.systemID
    else:
        speciesName = ""
        homeworldName=" no remaining homeworld "
        homeSystemID = -1
    if not speciesName:
        speciesName = foAI.foAIstate.origSpeciesName
    species = fo.getSpecies(speciesName)
    if not species:
        print "**************************************************************************************"
        print "**************************************************************************************"
        print "Problem determining species for colonization planning: capitalID: %s,  homeworld %s  and species name %s"%(capitalID,  homeworldName,  speciesName)
    else:
        print "Plannning colonization for species name %s"%species.name

    fleetSupplyableSystemIDs = empire.fleetSupplyableSystemIDs
    fleetSupplyablePlanetIDs = PlanetUtilsAI.getPlanetsInSystemsIDs(fleetSupplyableSystemIDs)
    print ""
    print "    fleetSupplyableSystemIDs: " + str(list(fleetSupplyableSystemIDs))
    print "    fleetSupplyablePlanetIDs: " + str(fleetSupplyablePlanetIDs)
    print ""

    print "-------\nEmpire Obstructed Starlanes:"
    print  list(empire.obstructedStarlanes())
    times.append( time() )
    tasks.append( "getting Empire Supply Info" )


    annexableSystemIDs.clear() #TODO: distinguish colony-annexable systems and outpost-annexable systems
    annexableRing1.clear()
    annexableRing2.clear()
    annexableRing3.clear()
    annexablePlanetIDs.clear()
    supply_distance = 1
    for tech in AIDependencies.supply_range_techs:
        if empire.getTechStatus(tech) == fo.techStatus.complete:
            supply_distance += 1 
    if foAI.foAIstate.aggression >= fo.aggression.aggressive:
        supply_distance += 1 
    for sysID in empire.fleetSupplyableSystemIDs:
        annexableSystemIDs.add(sysID) #add fleet supplyable system
        for nID in  universe.getImmediateNeighbors(sysID,  empireID):
            annexableSystemIDs.add(nID) #add immediate neighbor of fleet supplyable system
    if supply_distance > 1:
        for sysID in list(annexableSystemIDs):
            for nID in  universe.getImmediateNeighbors(sysID,  empireID):
                annexableRing1.add(nID)
        annexableRing1.difference_update(annexableSystemIDs)
        annexableSystemIDs.update(annexableRing1)
        print "First Ring of annexable systems: ",  PlanetUtilsAI.sysNameIDs(annexableRing1)
    if supply_distance > 2:
        for sysID in list(annexableRing1):
            for nID in  universe.getImmediateNeighbors(sysID,  empireID):
                annexableRing2.add(nID)
        annexableRing2.difference_update(annexableSystemIDs)
        print "Second Ring of annexable systems: ",  PlanetUtilsAI.sysNameIDs(annexableRing2)
        annexableSystemIDs.update(annexableRing2)
    if supply_distance > 3:
        for sysID in list(annexableRing2):
            for nID in  universe.getImmediateNeighbors(sysID,  empireID):
                annexableRing3.add(nID)
        annexableRing3.difference_update(annexableSystemIDs)
        print "Third Ring of annexable systems: ",  PlanetUtilsAI.sysNameIDs(annexableRing3)
        annexableSystemIDs.update(annexableRing3)
    annexablePlanetIDs.update( PlanetUtilsAI.getPlanetsInSystemsIDs(annexableSystemIDs))
    times.append( time() )
    tasks.append( "Determining Annexible Systems" )

    # get outpost and colonization planets

    exploredSystemIDs = foAI.foAIstate.getExplorableSystems(AIExplorableSystemType.EXPLORABLE_SYSTEM_EXPLORED)
    unExSysIDs = list(foAI.foAIstate.getExplorableSystems(AIExplorableSystemType.EXPLORABLE_SYSTEM_UNEXPLORED))
    unExSystems = map(universe.getSystem,  unExSysIDs)
    print "Unexplored Systems: %s " % [(sysID,  (sys and sys.name) or "name unknown") for sysID,  sys in zip( unExSysIDs,  unExSystems)]
    print "Explored SystemIDs: " + str(list(exploredSystemIDs))

    exploredPlanetIDs = PlanetUtilsAI.getPlanetsInSystemsIDs(exploredSystemIDs)
    print "Explored PlanetIDs: " + str(exploredPlanetIDs)
    print ""

    #visibleSystemIDs = foAI.foAIstate.visInteriorSystemIDs.keys() + foAI.foAIstate. visBorderSystemIDs.keys()
    #visiblePlanetIDs = PlanetUtilsAI.getPlanetsInSystemsIDs(visibleSystemIDs)
    #print "VisiblePlanets: %s "%[ (pid,  (universe.getPlanet(pid) and  universe.getPlanet(pid).name) or "unknown") for pid in  visiblePlanetIDs]
    #print ""

    #accessibleSystemIDs = [sysID for sysID in visibleSystemIDs if  universe.systemsConnected(sysID, homeSystemID, empireID) ]
    #acessiblePlanetIDs = PlanetUtilsAI.getPlanetsInSystemsIDs(accessibleSystemIDs)

    empireOwnedPlanetIDs = PlanetUtilsAI.getOwnedPlanetsByEmpire(universe.planetIDs, empireID)
    print "Empire Owned PlanetIDs:            " + str(empireOwnedPlanetIDs)

    #allOwnedPlanetIDs = PlanetUtilsAI.getAllOwnedPlanetIDs(exploredPlanetIDs) #working with Explored systems not all 'visible' because might not have a path to the latter
    allOwnedPlanetIDs = PlanetUtilsAI.getAllOwnedPlanetIDs(annexablePlanetIDs) #
    print "All annexable Owned or Populated PlanetIDs: " + str(set(allOwnedPlanetIDs)-set(empireOwnedPlanetIDs))

    #unOwnedPlanetIDs = list(set(exploredPlanetIDs) -set(allOwnedPlanetIDs))
    unOwnedPlanetIDs = list(set(annexablePlanetIDs) -set(allOwnedPlanetIDs))
    print "UnOwned annexable PlanetIDs:             ",  ",  ".join(PlanetUtilsAI.planetNameIDs(unOwnedPlanetIDs))

    colony_status['colonies_under_attack'] = []
    colony_status['colonies_under_threat'] = []
    empirePopCtrs = set( PlanetUtilsAI.getPopulatedPlanetIDs(  empireOwnedPlanetIDs) )
    empireOutpostIDs=set(empireOwnedPlanetIDs) - empirePopCtrs
    AIstate.popCtrIDs[:]=list(empirePopCtrs)
    AIstate.popCtrSystemIDs[:]=list(set(PlanetUtilsAI.getSystems(empirePopCtrs)))
    AIstate.outpostIDs[:]=list(empireOutpostIDs)
    AIstate.outpostSystemIDs[:]=list(set(PlanetUtilsAI.getSystems(empireOutpostIDs)))
    AIstate.colonizedSystems.clear()
    pop_map.clear()
    empire_status.clear()
    empire_status.update( {'industrialists':0,  'researchers':0} )
    times.append( time() )
    tasks.append( "Categorizing Visible Planets" )
    
    for pid in empireOwnedPlanetIDs:
        planet=universe.getPlanet(pid)
        if planet:
            AIstate.colonizedSystems.setdefault(planet.systemID,  []).append(pid)   # track these to plan Solar Generators and Singularity Generators, etc.
            if "ANCIENT_RUINS_SPECIAL" in planet.specials:
                gotRuins = True
            if planet.size == fo.planetSize.asteroids:
                gotAst = True
            elif planet.size == fo.planetSize.gasGiant:
                gotGG = True
            planetPopulation = planet.currentMeterValue(fo.meterType.population)
            pop_map[pid] = planetPopulation
            if planet.focus == EnumsAI.AIFocusType.FOCUS_INDUSTRY:
                empire_status['industrialists'] += planetPopulation
            elif planet.focus == EnumsAI.AIFocusType.FOCUS_RESEARCH:
                empire_status['researchers'] += planetPopulation
    AIstate.empireStars.clear()
    for sysID in AIstate.colonizedSystems:
        system = universe.getSystem(sysID)
        if system:
            AIstate.empireStars.setdefault(system.starType, []).append(sysID)
        sys_status = foAI.foAIstate.systemStatus.get(sysID,  {})
        if sys_status.get('fleetThreat') > 0:
            colony_status['colonies_under_attack'].append(sysID)
        if sys_status.get('neighborThreat') > 0:
            colony_status['colonies_under_threat'].append(sysID)

    claimedStars = {}
    for sType in AIstate.empireStars:
        claimedStars[sType] = list( AIstate.empireStars[sType] )
    for sysID in set( AIstate.colonyTargetedSystemIDs + AIstate.outpostTargetedSystemIDs):
        tSys = universe.getSystem(sysID)
        if not tSys: continue
        claimedStars.setdefault( tSys.starType, []).append(sysID)
    #foAI.foAIstate.misc['claimedStars'] = claimedStars


    oldPopCtrs=[]
    for specN in empireSpecies:
        oldPopCtrs.extend(empireSpecies[specN])
    oldEmpSpec = {}
    oldEmpSpec.update(empireSpecies)
    empireSpecies.clear()
    empireSpeciesByPlanet.clear()
    oldEmpCol= {}
    oldEmpCol.update(empireColonizers)
    empireColonizers.clear()
    empireShipBuilders.clear()
    empireShipyards.clear()
    empire_dry_docks.clear()
    empireMetabolisms.clear()
    availableGrowthSpecials.clear()
    activeGrowthSpecials.clear()
    empirePlanetsWithGrowthSpecials.clear()
    if empire.getTechStatus(TechsListsAI.EXOBOT_TECH_NAME) == fo.techStatus.complete:
        empireColonizers["SP_EXOBOT"]=[]# get it into colonizer list even if no colony yet
    empireSpeciesSystems.clear()

    for pID in empirePopCtrs:
        planet=universe.getPlanet(pID)
        if not planet:
            print "Error empire has apparently lost sight of former colony at planet %d but doesn't realize it"%pID
            continue
        pSpecName=planet.speciesName
        empireSpeciesByPlanet[pID] = pSpecName
        if pID not in oldPopCtrs:
            if  (AIFocusType.FOCUS_MINING in planet.availableFoci):
                fo.issueChangeFocusOrder(pID, AIFocusType.FOCUS_MINING)
                print "Changing focus of newly acquired planet ID %d : %s  to mining "%(pID,  planet.name ) #stale code left as an example; Mining no longer an option
        if planet.currentMeterValue(fo.meterType.population)>0 :
            empireSpecies.setdefault(pSpecName,  []).append(pID)
    pilotRatings.clear()
    print "\n"+"Empire species roster:"
    for specName in empireSpecies:
        thisSpec=fo.getSpecies(specName)
        pilotVal = 0
        if thisSpec:
            if thisSpec.canProduceShips:
                pilotVal = ratePilotingTag(list(thisSpec.tags) )
                if specName == "SP_ACIREMA":
                    pilotVal += 1
            thisMetab = [ tag for tag in thisSpec.tags if tag in  AIDependencies.metabolims]
            shipyards=[]
            for pID in empireSpecies[specName]:
                planet=universe.getPlanet(pID)
                if not planet: 
                    continue
                for metab in thisMetab:
                    #prev = empireMetabolisms.get(metab,  [0.0,  0.0] )
                    prev = empireMetabolisms.get(metab,  0.0 )
                    #prev[0] += planet.currentMeterValue(fo.meterType.population)
                    #prev[1] += planet.size
                    prev  += planet.size
                    empireMetabolisms[metab] = prev
                for special in planet.specials:
                    if special in AIDependencies.metabolimBoosts:
                        empirePlanetsWithGrowthSpecials.setdefault(pID,  []).append(special)
                        availableGrowthSpecials.setdefault(special,  []).append(pID)
                        if planet.focus == AIFocusType.FOCUS_GROWTH:
                            activeGrowthSpecials.setdefault(special,  []).append(pID)
                buildings_here = [universe.getObject(bldg).buildingTypeName for bldg in planet.buildingIDs]
                if  thisSpec.canProduceShips:
                    pilotRatings[pID] = pilotVal
                    if "BLD_SHIPYARD_BASE" in buildings_here:
                        shipyards.append(pID)
                if "BLD_SHIPYARD_ORBITAL_DRYDOCK" in buildings_here:
                    empire_dry_docks.setdefault(planet.systemID,  []).append(pID)
                empireSpeciesSystems.setdefault(planet.systemID,  {}).setdefault('pids', []).append(pID)
            if thisSpec.canProduceShips:
                empireShipBuilders[specName]=shipyards
                empireShipyards.update( [(yard, pilotVal) for yard in shipyards] )
                if thisSpec.canColonize:
                    empireColonizers[specName]=shipyards
            print "%s on planets %s; can%s colonize from %d shipyards; has tags %s"%(specName,  empireSpecies[specName],  ["not", ""][thisSpec.canColonize and thisSpec.canProduceShips], len(shipyards),  list(thisSpec.tags))
        else:
            print "Unable to retrieve info for Species named %s"%specName
    print""
    if len(pilotRatings) != 0:
        ratingList=sorted(pilotRatings.values(), reverse=True)
        curBestPilotRating = ratingList[0]
        if len(pilotRatings) == 1:
            curMidPilotRating = ratingList[0]
        else:
            curMidPilotRating = ratingList[1+int(len(ratingList)/5)]
    else:
        curBestPilotRating = 1e-8
        curMidPilotRating = 1e-8
    if empireSpecies!=oldEmpSpec:
        print "Old empire species: %s  ; new empire species: %s"%(oldEmpSpec,  empireSpecies)
    if empireColonizers!=oldEmpCol:
        print "Old empire colonizers: %s  ; new empire colonizers: %s"%(oldEmpCol,  empireColonizers)

    print
    times.append( time() )
    tasks.append( "Evaluating Existing Colonies" )

    # export colony targeted systems for other AI modules
    colonyTargetedPlanetIDs = getColonyTargetedPlanetIDs(universe.planetIDs, AIFleetMissionType.FLEET_MISSION_COLONISATION, empireID)
    allColonyTargetedSystemIDs = PlanetUtilsAI.getSystems(colonyTargetedPlanetIDs)
    AIstate.colonyTargetedSystemIDs = allColonyTargetedSystemIDs
    print ""
    print "Colony Targeted SystemIDs:         " + str(AIstate.colonyTargetedSystemIDs)
    print "Colony Targeted PlanetIDs:         " + str(colonyTargetedPlanetIDs)

    colonyFleetIDs = FleetUtilsAI.getEmpireFleetIDsByRole(AIFleetMissionType.FLEET_MISSION_COLONISATION)
    if not colonyFleetIDs:
        print "Available Colony Fleets:             0"
    else:
        print "Colony FleetIDs:                   " + str(FleetUtilsAI.getEmpireFleetIDsByRole(AIFleetMissionType.FLEET_MISSION_COLONISATION))

    numColonyFleets = len(FleetUtilsAI.extractFleetIDsWithoutMissionTypes(colonyFleetIDs))
    print "Colony Fleets Without Missions:      " + str(numColonyFleets)



    outpostTargetedPlanetIDs = getOutpostTargetedPlanetIDs(universe.planetIDs, AIFleetMissionType.FLEET_MISSION_OUTPOST, empireID)
    outpostTargetedPlanetIDs.extend( getOutpostTargetedPlanetIDs(universe.planetIDs, AIFleetMissionType.FLEET_MISSION_ORBITAL_OUTPOST, empireID))
    allOutpostTargetedSystemIDs = PlanetUtilsAI.getSystems(outpostTargetedPlanetIDs)

    # export outpost targeted systems for other AI modules
    AIstate.outpostTargetedSystemIDs = allOutpostTargetedSystemIDs
    print ""
    print "Outpost Targeted SystemIDs:        " + str(AIstate.outpostTargetedSystemIDs)
    print "Outpost Targeted PlanetIDs:        " + str(outpostTargetedPlanetIDs)

    outpostFleetIDs = FleetUtilsAI.getEmpireFleetIDsByRole(AIFleetMissionType.FLEET_MISSION_OUTPOST)
    if not outpostFleetIDs:
        print "Available Outpost Fleets:            0"
    else:
        print "Outpost FleetIDs:                  " + str(FleetUtilsAI.getEmpireFleetIDsByRole(AIFleetMissionType.FLEET_MISSION_OUTPOST))

    numOutpostFleets = len(FleetUtilsAI.extractFleetIDsWithoutMissionTypes(outpostFleetIDs))
    print "Outpost Fleets Without Missions:     " + str(numOutpostFleets)
    times.append( time() )
    tasks.append( "Identify Existing colony/outpost targets" )

    availablePP = dict(  [ (tuple(el.key()),  el.data() ) for el in  empire.planetsWithAvailablePP ])  #keys are sets of ints; data is doubles
    availPP_BySys={}
    for pSet in availablePP:
        availPP_BySys.update(  [ (sysID, availablePP[pSet]) for sysID in  set(PlanetUtilsAI.getSystems( pSet))] )
    colonyCost= AIDependencies.colonyPodCost *(1+ AIDependencies.colonyPodUpkeep *len( list(AIstate.popCtrIDs) ))
    outpostCost= AIDependencies.outpostPodCost *(1+ AIDependencies.colonyPodUpkeep *len( list(AIstate.popCtrIDs) ))
    productionQueue = empire.productionQueue
    queued_outpost_bases=[]
    queued_colony_bases=[]
    for queue_index  in range(0,  len(productionQueue)):
        element=productionQueue[queue_index]
        if element.buildType == EnumsAI.AIEmpireProductionTypes.BT_SHIP:
            if foAI.foAIstate.getShipRole(element.designID) in  [ EnumsAI.AIShipRoleType.SHIP_ROLE_BASE_OUTPOST ] :
                buildPlanet = universe.getPlanet(element.locationID)
                queued_outpost_bases.append(buildPlanet.systemID)
            elif foAI.foAIstate.getShipRole(element.designID) in  [ EnumsAI.AIShipRoleType.SHIP_ROLE_BASE_COLONISATION ] :
                buildPlanet = universe.getPlanet(element.locationID)
                queued_colony_bases.append(buildPlanet.systemID)

    evaluatedColonyPlanetIDs = list(set(unOwnedPlanetIDs).union(empireOutpostIDs) - set(colonyTargetedPlanetIDs) ) # places for possible colonyBase
    
    #foAI.foAIstate.qualifyingOutpostBaseTargets.clear() #don't want to lose the info by clearing, but #TODO: should double check if still own colonizer planet
    #foAI.foAIstate.qualifyingColonyBaseTargets.clear()
    cost_ratio = 120 #TODO: temp ratio; reest to 12 *; consider different ratio
    for pid in evaluatedColonyPlanetIDs: #TODO: reorganize
        planet = universe.getPlanet(pid)
        if not planet: 
            continue
        sysID = planet.systemID
        for pid2 in empireSpeciesSystems.get(sysID,  {}).get('pids', []):
            planet2 = universe.getPlanet(pid2)
            if not planet2: 
                continue
            if (pid not in  foAI.foAIstate.qualifyingOutpostBaseTargets) and (planet2.speciesName  in empireColonizers):
                if  (pid not in empireOutpostIDs): 
                    foAI.foAIstate.qualifyingOutpostBaseTargets.setdefault(pid,  [pid2,  -1])
            if (pid not in  foAI.foAIstate.qualifyingColonyBaseTargets) and (planet2.speciesName  in empireColonizers):
                if  colonyCost <  cost_ratio * availPP_BySys.get(sysID,  0): 
                    foAI.foAIstate.qualifyingColonyBaseTargets.setdefault(pid,  [pid2,  -1])#TODO: check other local colonizers for better score

    # print "Evaluated Colony PlanetIDs:        " + str(evaluatedColonyPlanetIDs)
    times.append( time() )
    tasks.append( "identify colony base targets" )

    reserved_outpost_base_targets = foAI.foAIstate.qualifyingOutpostBaseTargets.keys()
    max_queued_outpost_bases = max(1,  int(2*empire.productionPoints / outpostCost))
    print "considering possible outpost bases at %s" % (reserved_outpost_base_targets)
    if ( empire.getTechStatus(AIDependencies.outposting_tech) == fo.techStatus.complete):
        for pid in (set(reserved_outpost_base_targets) - set(outpostTargetedPlanetIDs)):
            if len(queued_outpost_bases) >=max_queued_outpost_bases:
                print "too many queued outpost bases to build any more now"
                break
            if pid not in unOwnedPlanetIDs: 
                continue
            if  foAI.foAIstate.qualifyingOutpostBaseTargets[pid][1] != -1: 
                continue  #already building for here
            loc = foAI.foAIstate.qualifyingOutpostBaseTargets[pid][0]
            this_score = evaluatePlanet(pid,  EnumsAI.AIFleetMissionType.FLEET_MISSION_OUTPOST,  fleetSupplyablePlanetIDs,  None,  empire, [])
            planet = universe.getPlanet(pid)
            if this_score == 0:
                #print "Potential outpost base (rejected) for %s to be built at planet id(%d); outpost score %.1f" % ( ((planet and planet.name) or "unknown"), loc,  this_score)
                continue
            print "Potential outpost base for %s to be built at planet id(%d); outpost score %.1f" % ( ((planet and planet.name) or "unknown"), loc,  this_score)
            if this_score < 100:
                print "Potential outpost base (rejected) for %s to be built at planet id(%d); outpost score %.1f" % ( ((planet and planet.name) or "unknown"), loc,  this_score)
                continue
            bestShip,  colDesign,  buildChoices = ProductionAI.getBestShipInfo(EnumsAI.AIPriorityType.PRIORITY_PRODUCTION_ORBITAL_OUTPOST,  loc)
            if not bestShip:
                print "Error: no outpost base can be built at ",  PlanetUtilsAI.planetNameIDs([loc])
                continue
            #print "selecting  ",  PlanetUtilsAI.planetNameIDs([pid]),  " to build Orbital Defenses"
            retval  = fo.issueEnqueueShipProductionOrder(bestShip, loc)
            print "Enqueueing Outpost Base at %s for %s"%( PlanetUtilsAI.planetNameIDs([loc]),  PlanetUtilsAI.planetNameIDs([pid]))
            if retval !=0:
                foAI.foAIstate.qualifyingOutpostBaseTargets[pid][1] = loc
                queued_outpost_bases.append((planet and planet.systemID) or -1)
                #res=fo.issueRequeueProductionOrder(productionQueue.size -1,  0) #TODO: evaluate move to front
    times.append( time() )
    tasks.append( "initiate outpost base construction" )

    evaluatedOutpostPlanetIDs = list(set(unOwnedPlanetIDs) - set(outpostTargetedPlanetIDs)- set(colonyTargetedPlanetIDs) - set(reserved_outpost_base_targets))

    # print "Evaluated Outpost PlanetIDs:       " + str(evaluatedOutpostPlanetIDs)

    evaluatedColonyPlanets = assignColonisationValues(evaluatedColonyPlanetIDs, AIFleetMissionType.FLEET_MISSION_COLONISATION, fleetSupplyablePlanetIDs, None, empire)
    times.append( time() )
    tasks.append( "Evaluate %d Primary Colony Opportunities"%(len( evaluatedOutpostPlanetIDs )) )
    allColonyOpportunities.clear()
    allColonyOpportunities.update(assignColonisationValues(evaluatedColonyPlanetIDs, AIFleetMissionType.FLEET_MISSION_COLONISATION, fleetSupplyablePlanetIDs, None, empire,  [], True))
    times.append( time() )
    tasks.append( "Evaluate All Colony Opportunities" )

    sortedPlanets = evaluatedColonyPlanets.items()
    sortedPlanets.sort(lambda x, y: cmp(x[1], y[1]), reverse=True)

    print ""
    print "Settleable Colony Planets (score,species) | ID | Name | Specials:"
    for ID, score in sortedPlanets:
        if score > 0.5:
            print "   %15s | %5s  | %s  | %s "%(score,  ID,  universe.getPlanet(ID).name ,  list(universe.getPlanet(ID).specials))
    print ""

    sortedPlanets = [(ID, score) for ID, score in sortedPlanets if score[0] > 0]
    # export planets for other AI modules
    foAI.foAIstate.colonisablePlanetIDs = sortedPlanets

    # get outpost fleets
    allOutpostFleetIDs = FleetUtilsAI.getEmpireFleetIDsByRole(AIFleetMissionType.FLEET_MISSION_OUTPOST)
    AIstate.outpostFleetIDs = FleetUtilsAI.extractFleetIDsWithoutMissionTypes(allOutpostFleetIDs)

    evaluatedOutpostPlanets = assignColonisationValues(evaluatedOutpostPlanetIDs, AIFleetMissionType.FLEET_MISSION_OUTPOST, fleetSupplyablePlanetIDs, None, empire)
    #removeLowValuePlanets(evaluatedOutpostPlanets)
    times.append( time() )
    tasks.append( "Evaluate Outpost Opportunities" )

    sortedOutposts = evaluatedOutpostPlanets.items()
    sortedOutposts.sort(lambda x, y: cmp(x[1], y[1]), reverse=True)

    print "Settleable Outpost PlanetIDs:"
    for ID, score in sortedOutposts:
        if score > 0.5:
            print "   %5s | %5s  | %s  | %s "%(score,  ID,  universe.getPlanet(ID).name ,  list(universe.getPlanet(ID).specials))
    print ""

    sortedOutposts = [(ID, score) for ID, score in sortedOutposts if score[0] > 0]
    # export outposts for other AI modules
    foAI.foAIstate.colonisableOutpostIDs = sortedOutposts
    times.append( time() )
    tasks.append( "total processing" )
    for t_index in range(1, len(times)-1):
        print "getColonyFleets(): %40s took %d msec"%(tasks[t_index],  int(1000*(times[t_index]-times[t_index-1])))
    print "getColonyFleets(): %40s took %d msec"%(tasks[-1],  int(1000*(times[-1]-times[0])))

def getColonyTargetedPlanetIDs(planetIDs, missionType, empireID):
    "return list being settled with colony planets"

    universe = fo.getUniverse()
    colonyAIFleetMissions = foAI.foAIstate.getAIFleetMissionsWithAnyMissionTypes([missionType])

    colonyTargetedPlanets = []

    for planetID in planetIDs:
        planet = universe.getPlanet(planetID)
        # add planets that are target of a mission
        for colonyAIFleetMission in colonyAIFleetMissions:
            aiTarget = AITarget.AITarget(AITargetType.TARGET_PLANET, planetID)
            if colonyAIFleetMission.hasTarget(missionType, aiTarget):
                colonyTargetedPlanets.append(planetID)

    return colonyTargetedPlanets

def getOutpostTargetedPlanetIDs(planetIDs, missionType, empireID):
    "return list being settled with outposts planets"

    universe = fo.getUniverse()
    outpostAIFleetMissions = foAI.foAIstate.getAIFleetMissionsWithAnyMissionTypes([missionType])

    outpostTargetedPlanets = []

    for planetID in planetIDs:
        planet = universe.getPlanet(planetID)
        # add planets that are target of a mission
        for outpostAIFleetMission in outpostAIFleetMissions:
            aiTarget = AITarget.AITarget(AITargetType.TARGET_PLANET, planetID)
            if outpostAIFleetMission.hasTarget(missionType, aiTarget):
                outpostTargetedPlanets.append(planetID)

    return outpostTargetedPlanets

def assignColonisationValues(planetIDs, missionType, fleetSupplyablePlanetIDs, species, empire,  detail=None,  returnAll=False): #TODO: clean up supplyable versus annexable
    "creates a dictionary that takes planetIDs as key and their colonisation score as value"
    if detail is None:
        detail = []
    origDetail = detail
    planetValues = {}
    if   (missionType == AIFleetMissionType.FLEET_MISSION_OUTPOST ):
        #print "\n=========\nAssigning Outpost Values\n========="
        trySpecies = [ "" ]
    elif species is not None:
        #print "\n=========\nAssigning Colony Values\n========="
        if isinstance(species,  str):
            trySpecies = [species]
        elif isinstance(species,  list):
            trySpecies = species
        else:
            trySpecies = [species.name]
    else:
        #print "\n=========\nAssigning Colony Values\n========="
        trySpecies = list(  empireColonizers  )
    for planetID in planetIDs:
        pv = []
        for specName in trySpecies:
            detail = origDetail[:]
            pv.append( (evaluatePlanet(planetID, missionType, fleetSupplyablePlanetIDs, specName, empire,  detail),  specName,  list(detail)) )
        allSorted = sorted(pv,  reverse=True)
        best = allSorted[:1]
        if best!=[]:
            if returnAll:
                planetValues[planetID] = allSorted
            else:
                planetValues[planetID] = best[0][:2]
                #print best[0][2]
    return planetValues
    
def next_turn_pop_change(cur_pop,  target_pop):
    "population change calc taken from PopCenter.cpp"
    pop_change = 0
    if (target_pop > cur_pop):
        pop_change = cur_pop * (target_pop + 1 - cur_pop) / 100
        pop_change = min(pop_change, target_pop - cur_pop)
    else:
        pop_change = -(cur_pop - target_pop) / 10
        pop_change = max(pop_change, target_pop - cur_pop)
    return pop_change

def project_ind_val(init_pop,  max_pop_size,  init_industry,  max_ind_factor,  flat_industry,  discountMultiplier):
    "returns a discouted value for a projected industry stream over time with changing population"
    discount_factor = 0.95
    if discountMultiplier > 1.0:
        discount_factor = 1.0 - 1.0/discountMultiplier
    cur_pop = float(init_pop)
    cur_ind = float(init_industry)
    ind_val = 0
    val_factor = 1.0
    for turn in range(50):
        cur_pop += next_turn_pop_change(cur_pop,  max_pop_size)
        cur_ind =  min(cur_ind + 1,  max(0,  cur_ind - 1,  flat_industry + cur_pop * max_ind_factor))
        val_factor *= discount_factor
        ind_val += val_factor * cur_ind
    return ind_val

def evaluatePlanet(planetID, missionType, fleetSupplyablePlanetIDs, specName, empire,  detail = None):
    "returns the colonisation value of a planet"
    if detail is None:
        detail = []
    retval = 0
    discountMultiplier = [30.0, 40.0][ fo.empireID() % 2 ]
    species=fo.getSpecies(specName or "") #in case None is passed as specName
    pilotVal = 0
    if species and species.canProduceShips:
        pilotVal = ratePilotingTag(species.tags)
        if pilotVal > curBestPilotRating:
            pilotVal *= 2
        if pilotVal > 2:
            retval += discountMultiplier*5*pilotVal
            detail.append("Pilot Val %.1f"%(discountMultiplier*5*pilotVal))

    if detail != []:
        detail = []
    priorityScaling=1.0
    maxGGGs=1 #if goes above 1 need some other adjustments below
    if empire.productionPoints <100:
        backupFactor = 0.0
    else:
        backupFactor = min(1.0,  (empire.productionPoints/200.0)**2 )

    universe = fo.getUniverse()
    claimedStars= foAI.foAIstate.misc.get('claimedStars',  {} )
    if claimedStars == {}:
        for sType in AIstate.empireStars:
            claimedStars[sType] = list( AIstate.empireStars[sType] )
        for sysID in set( AIstate.colonyTargetedSystemIDs + AIstate.outpostTargetedSystemIDs):
            tSys = universe.getSystem(sysID)
            if not tSys: continue
            claimedStars.setdefault( tSys.starType, []).append(sysID)

    empireResearchList = [element.tech for element in empire.researchQueue]
    planet = universe.getPlanet(planetID)
    if (planet == None):
        VisMap = dictFromMap(universe.getVisibilityTurnsMap(planetID,  empire.empireID))
        print "Planet %d object not available; visMap: %s"%(planetID,  VisMap)
        return 0
    detail.append("%s : "%planet.name )
    this_sysid = planet.systemID
    haveExistingPresence=False
    if  AIstate.colonizedSystems.get(this_sysid,  []) not in [ [],  [planetID]]: #if existing presence is target planet, don't count
        haveExistingPresence=True
    system = universe.getSystem(this_sysid)
    sys_status = foAI.foAIstate.systemStatus.get(planet.systemID,  {})
    myrating = sys_status.get('myFleetRating', 0)
    fleet_threat_ratio = (sys_status.get('fleetThreat', 0) - myrating ) / float(curBestMilShipRating)
    monster_threat_ratio = sys_status.get('monsterThreat', 0) / float(curBestMilShipRating)
    neighbor_threat_ratio = ((sys_status.get('neighborThreat', 0) )  / float(curBestMilShipRating)) + min( 0,  fleet_threat_ratio ) # last portion gives credit for inner extra defenses
    myrating = sys_status.get('my_neighbor_rating', 0)
    jump2_threat_ratio = ((sys_status.get('jump2_threat', 0) - myrating ) / float(curBestMilShipRating)) + min( 0,  neighbor_threat_ratio ) # last portion gives credit for inner extra defenses

    thrtFactor = 1.0
    ship_limit = 2 * (2**(fo.currentTurn() / 40.0))
    threat_tally = fleet_threat_ratio + neighbor_threat_ratio + monster_threat_ratio
    if haveExistingPresence:
        threat_tally += 0.3 * jump2_threat_ratio
        threat_tally *= 0.8
    else:
        threat_tally += 0.6 * jump2_threat_ratio
    if ( threat_tally ) > ship_limit:
        thrtFactor = 0.1
    elif ( fleet_threat_ratio + neighbor_threat_ratio + monster_threat_ratio ) > 0.6*ship_limit:
        thrtFactor = 0.4
    elif ( fleet_threat_ratio + neighbor_threat_ratio + monster_threat_ratio ) > 0.2*ship_limit:
        thrtFactor = 0.8


    sysPartialVisTurn = dictFromMap(universe.getVisibilityTurnsMap(this_sysid,  empire.empireID)).get(fo.visibility.partial, -9999)
    planetPartialVisTurn = dictFromMap(universe.getVisibilityTurnsMap(planetID,  empire.empireID)).get(fo.visibility.partial, -9999)

    if planetPartialVisTurn < sysPartialVisTurn:
        print "Colonization AI couldn't get current info on planet id %d (was stealthed at last sighting)"%planetID
        return 0  #last time we had partial vis of the system, the planet was stealthed to us #TODO: track detection strength, order new scouting when it goes up
    
    tagList=[]
    starBonus=0
    colonyStarBonus=0
    researchBonus=0
    growthVal = 0
    fixedInd = 0
    fixedRes = 0
    if species:
        tagList = list( species.tags )
    starPopMod=0
    if system:
        alreadyGotThisOne= this_sysid in (AIstate.popCtrSystemIDs + AIstate.outpostSystemIDs)
        if "PHOTOTROPHIC" in tagList:
            starPopMod = photoMap.get( system.starType,  0 )
            detail.append( "PHOTOTROPHIC popMod %.1f"%starPopMod    )
        if (empire.getTechStatus("PRO_SOL_ORB_GEN") == fo.techStatus.complete) or (  "PRO_SOL_ORB_GEN"  in empireResearchList[:5])  :
            if system.starType in [fo.starType.blue, fo.starType.white]:
                if len (claimedStars.get(fo.starType.blue,  [])+claimedStars.get(fo.starType.white,  []))==0:
                    starBonus +=20* discountMultiplier
                    detail.append( "PRO_SOL_ORB_GEN BW  %.1f"%(20* discountMultiplier)    )
                elif   not alreadyGotThisOne:
                    starBonus +=10*discountMultiplier*backupFactor #still has extra value as an alternate location for solar generators
                    detail.append( "PRO_SOL_ORB_GEN BW Backup Location  %.1f"%(10* discountMultiplier *backupFactor)   )
                elif fo.currentTurn() > 100:  #lock up this whole system
                    pass
                    #starBonus += 5 #TODO: how much?
                    #detail.append( "PRO_SOL_ORB_GEN BW LockingDownSystem   %.1f"%5  )
            if system.starType in [fo.starType.yellow, fo.starType.orange]:
                if len (     claimedStars.get(fo.starType.blue,  [])+claimedStars.get(fo.starType.white,  [])+
                                    claimedStars.get(fo.starType.yellow,  [])+claimedStars.get(fo.starType.orange,  []))==0:
                    starBonus +=10* discountMultiplier
                    detail.append( "PRO_SOL_ORB_GEN YO  %.1f"%(10* discountMultiplier ))
                else:
                    pass
                    #starBonus +=2 #still has extra value as an alternate location for solar generators
                    #detail.append( "PRO_SOL_ORB_GEN YO Backup  %.1f"%2 )
        if system.starType in [fo.starType.blackHole] and fo.currentTurn() > 100:
            if not alreadyGotThisOne:
                starBonus +=10*discountMultiplier*backupFactor #whether have tech yet or not, assign some base value
                detail.append( "Black Hole %.1f"%(10* discountMultiplier*backupFactor)    )
            else:
                starBonus += 5*discountMultiplier*backupFactor
                detail.append( "Black Hole Backup %.1f"%(5* discountMultiplier*backupFactor )   )
        if (empire.getTechStatus("PRO_SINGULAR_GEN") == fo.techStatus.complete) or (  "PRO_SINGULAR_GEN"  in empireResearchList[:8])  :
            if system.starType in [fo.starType.blackHole] :
                if len (claimedStars.get(fo.starType.blackHole,  []))==0:
                    starBonus +=200*discountMultiplier #pretty rare planets, good for generator
                    detail.append( "PRO_SINGULAR_GEN %.1f"%(200* discountMultiplier  )  )
                elif  this_sysid not in claimedStars.get(fo.starType.blackHole,  []):
                    starBonus +=100*discountMultiplier*backupFactor #still has extra value as an alternate location for generators & for blocking enemies generators
                    detail.append( "PRO_SINGULAR_GEN Backup %.1f"%(100* discountMultiplier*backupFactor  )  )
            elif system.starType in [fo.starType.red] and ( len (claimedStars.get(fo.starType.blackHole,  [])) )==0:
                rfactor = (1.0+len (claimedStars.get(fo.starType.red,  [])))**(-2)
                starBonus +=40*discountMultiplier*backupFactor*rfactor # can be used for artificial black hole
                detail.append( "Red Star for Art Black Hole  %.1f"%(20* discountMultiplier*backupFactor*rfactor  )  )
        if (empire.getTechStatus("PRO_NEUTRONIUM_EXTRACTION") == fo.techStatus.complete) or (  "PRO_NEUTRONIUM_EXTRACTION"  in empireResearchList[:8])  :
            if system.starType in [fo.starType.neutron]:
                if len (claimedStars.get(fo.starType.neutron,  []))==0:
                    starBonus +=80*discountMultiplier #pretty rare planets, good for armor
                    detail.append( "PRO_NEUTRONIUM_EXTRACTION  %.1f"%(80* discountMultiplier  )  )
                else:
                    starBonus +=20*discountMultiplier*backupFactor #still has extra value as an alternate location for generators & for bnlocking enemies generators
                    detail.append( "PRO_NEUTRONIUM_EXTRACTION Backup  %.1f"%(20* discountMultiplier*backupFactor  )  )
        if (empire.getTechStatus("SHP_ENRG_BOUND_MAN") == fo.techStatus.complete) or (  "SHP_ENRG_BOUND_MAN"  in empireResearchList[:6])  :
            if system.starType in [fo.starType.blackHole,  fo.starType.blue] :
                initVal=100*discountMultiplier*(pilotVal or 1)
                if len (claimedStars.get(fo.starType.blackHole,  [])  +  claimedStars.get(fo.starType.blue,  []) )    ==0:
                    colonyStarBonus += initVal #pretty rare planets, good for energy shipyards
                    detail.append( "SHP_ENRG_BOUND_MAN  %.1f"%(initVal  )  )
                elif  this_sysid not in (claimedStars.get(fo.starType.blackHole,  [])  +  claimedStars.get(fo.starType.blue,  []) ):
                    colonyStarBonus +=0.5*initVal*backupFactor #still has extra value as an alternate location for energy shipyard
                    detail.append( "SHP_ENRG_BOUND_MAN Backup  %.1f"%(0.5*initVal*backupFactor  )  )
    retval +=starBonus

    planetSpecials = list(planet.specials)
    if "ECCENTRIC_ORBIT_SPECIAL" in planet.specials:
        fixedRes += discountMultiplier*2*3
        detail.append( "ECCENTRIC_ORBIT_SPECIAL  %.1f"%(discountMultiplier*2*3  )  )

    if   (missionType == AIFleetMissionType.FLEET_MISSION_OUTPOST ):

        if ( "ANCIENT_RUINS_SPECIAL" in planet.specials ): #TODO: add value for depleted ancient ruins
            retval += discountMultiplier*30
            detail.append("Undepleted Ruins %.1f"%discountMultiplier*30)

        for special in planetSpecials:
            nestVal = 0
            if "_NEST_" in special:
                nestVal = nestValMap.get(special,  5) * discountMultiplier*backupFactor # get an outpost on the nest quick
                retval+=nestVal
                detail.append( "%s  %.1f"%(special,  nestVal  )  )
        if   ( planet.size  ==  fo.planetSize.asteroids ):
            astVal = 0
            if (empire.getTechStatus("PRO_MICROGRAV_MAN") == fo.techStatus.complete ):
                per_ast = 5
            else:
                per_ast = 2.5
            if system:
                for pid in system.planetIDs:
                    otherPlanet=universe.getPlanet(pid)
                    if otherPlanet.size == fo.planetSize.asteroids:
                        if pid==planetID:
                            continue
                        elif pid < planetID and planet.unowned:
                            astVal=0
                            break
                    elif otherPlanet.speciesName!="": #and otherPlanet.owner==empire.empireID
                        astVal+= per_ast * discountMultiplier
                retval += astVal
                if astVal >0:
                    detail.append( "AsteroidMining %.1f"%(astVal  )  )
            astVal=0
            if (empire.getTechStatus("SHP_ASTEROID_HULLS") == fo.techStatus.complete ) : 
                per_ast = 20
            elif (empire.getTechStatus("CON_ORBITAL_CON") == fo.techStatus.complete ) : 
                per_ast = 10
            else:
                per_ast = 5
            if system:
                for pid in system.planetIDs:
                    otherPlanet=universe.getPlanet(pid)
                    if otherPlanet.size == fo.planetSize.asteroids:
                        if pid==planetID:
                            continue
                        elif pid < planetID  and planet.unowned:
                            astVal=0
                            break
                    elif otherPlanet.speciesName!="": #and otherPlanet.owner==empire.empireID
                        otherSpecies = fo.getSpecies(otherPlanet.speciesName)
                        if otherSpecies and otherSpecies.canProduceShips:
                            astVal+= per_ast * discountMultiplier
                retval += astVal
                if astVal >0:
                    detail.append( "AsteroidShipBuilding %.1f"%(astVal  )  )
        if  ( ( planet.size  ==  fo.planetSize.gasGiant ) and (empire.getTechStatus("PRO_ORBITAL_GEN") == fo.techStatus.complete ) ):
            per_GG = 20
        elif  ( ( planet.size  ==  fo.planetSize.gasGiant ) and (empire.getTechStatus("CON_ORBITAL_CON") == fo.techStatus.complete ) ):
            per_GG = 10
        else:
            per_GG = 5
        if system:
            GGList=[]
            orbGenVal=0
            GGDetail=[]
            for pid in system.planetIDs:
                otherPlanet=universe.getPlanet(pid)
                if otherPlanet.size== fo.planetSize.gasGiant:
                    GGList.append(pid)
                if  pid!=planetID and otherPlanet.owner==empire.empireID and (AIFocusType.FOCUS_INDUSTRY  in list(otherPlanet.availableFoci)+[otherPlanet.focus]):
                    orbGenVal+= per_GG*discountMultiplier
                    GGDetail.append( "GGG  for %s  %.1f"%(otherPlanet.name,    discountMultiplier*per_GG  )  )
            if planetID in sorted(GGList)[:maxGGGs]:
                retval += orbGenVal
                detail.extend( GGDetail )
            else:
                detail.append( "Won't GGG")
        if thrtFactor < 1.0:
            retval *=thrtFactor
            detail.append( "threat reducing value by %3d %%"%(100*(1-thrtFactor)))
        if haveExistingPresence:
            detail.append("preexisting system colony")
            retval *=1.5
        return int(retval)
    else: #colonization mission
        if not species:
            return 0

        if ( "ANCIENT_RUINS_SPECIAL" in planet.specials ):
            retval += discountMultiplier*50
            detail.append("Undepleted Ruins %.1f"%discountMultiplier*50)

        popTagMod = 1.0
        indTagMod = 1.0
        resTagMod = 1.0
        AITags=""
        for tag in [tag1 for tag1 in tagList if "AI_TAG" in tag1]:
            tagParts = tag.split('_')
            tagType = tagParts[3]
            AITags +=  " AI_TAG: %s "%tag
            grade = {'NO':0.0, 'BAD':0.5,  'GOOD':1.5,  'GREAT':2.0,  'ULTIMATE':4.0 }.get(tagParts[2],  1.0)
            if tagType == "POPULATION":
                popTagMod = {'BAD':0.75,  'GOOD':1.25}.get(tagParts[2],  1.0)
            elif tagType =="INDUSTRY":
                indTagMod = grade
            elif tagType =="RESEARCH":
                resTagMod = grade

        #if AITags != "":
        #    print "Species %s has AITags %s"%(specName,  AITags)

        retval += fixedRes
        retval += colonyStarBonus
        asteroidBonus=0
        gasGiantBonus=0
        flat_industry = 0
        miningBonus=0
        perGGG=10
        planetSize = planet.size

        gotAsteroids=False
        gotOwnedAsteroids = False
        got_GG = False
        gotOwnedGG = False
        if system and AIFocusType.FOCUS_INDUSTRY in species.foci:
            for pid  in [temp_id for temp_id in system.planetIDs if temp_id != planetID]:
                p2 = universe.getPlanet(pid)
                if p2:
                    if p2.size== fo.planetSize.asteroids:
                        gotAsteroids = True
                        if p2.owner != -1:
                            gotOwnedAsteroids = True
                    if p2.size== fo.planetSize.gasGiant:
                        got_GG = True
                        if p2.owner != -1:
                            gotOwnedGG = True
        if gotAsteroids:
            if ( (empire.getTechStatus("PRO_MICROGRAV_MAN") == fo.techStatus.complete ) or (  "PRO_MICROGRAV_MAN"  in empireResearchList[:10]) ):
                if gotOwnedAsteroids: #can be quickly captured
                    flat_industry += 5 #will go into detailed industry projection
                    detail.append( "Asteroid mining ~ %.1f"%(5*discountMultiplier  )  )
                else: #uncertain when can be outposted
                    asteroidBonus = 2.5*discountMultiplier #give partial value
                    detail.append( "Asteroid mining %.1f"%(5*discountMultiplier  )  )
            if (empire.getTechStatus("SHP_ASTEROID_HULLS") == fo.techStatus.complete ) or (  "SHP_ASTEROID_HULLS"  in empireResearchList[:11]) :
                if species and species.canProduceShips:
                    asteroidBonus += 30*discountMultiplier*pilotVal
                    detail.append( "Asteroid ShipBuilding from %s  %.1f"%(p2.name,    discountMultiplier*20*pilotVal  )  )
        if got_GG:
            if ( (empire.getTechStatus("PRO_ORBITAL_GEN") == fo.techStatus.complete ) or (  "PRO_ORBITAL_GEN"  in empireResearchList[:5]) ):
                if gotOwnedGG:
                    flat_industry += perGGG #will go into detailed industry projection
                    detail.append( "GGG ~  %.1f"%( perGGG * discountMultiplier  )  )
                else:
                    gasGiantBonus = 0.5 * perGGG * discountMultiplier
                    detail.append( "GGG  %.1f"%( 0.5 * perGGG * discountMultiplier  )  )
        if   (planet.size==fo.planetSize.gasGiant):
            if not (species and species.name  ==  "SP_SUPER_TEST"):
                detail.append("Can't Settle GG" )
                return 0
            else:
                planetEnv = fo.planetEnvironment.adequate#I think
                planetSize=6 #I think
        elif ( planet.size  ==  fo.planetSize.asteroids ):
            planetSize=3 #I think
            if  not species or (species.name not in [  "SP_EXOBOT", "SP_SUPER_TEST"  ]):
                detail.append( "Can't settle Asteroids" )
                return 0
            elif species.name == "SP_EXOBOT":
                planetEnv =fo.planetEnvironment.poor
            elif species.name == "SP_SUPER_TEST":
                planetEnv = fo.planetEnvironment.adequate#I think
            else:
                return 0
        else:
            planetEnv  = environs[ str(species.getPlanetEnvironment(planet.type)) ]
        if planetEnv==0:
            return -9999
        popSizeMod=0
        conditionalPopSizeMod=0
        postPopSizeMod=0
        popSizeMod += popSizeModMap["env"][planetEnv]
        detail.append("EnvironPopSizeMod(%d)"%popSizeMod)
        if "SELF_SUSTAINING" in tagList:
            popSizeMod*=2
            detail.append("SelfSustaining_PSM(2)" )
        if "PHOTOTROPHIC" in tagList:
            popSizeMod += starPopMod
            detail.append("Phototropic Star Bonus_PSM(%0.1f)"%starPopMod)
        if (empire.getTechStatus("GRO_SUBTER_HAB") == fo.techStatus.complete)  or "TUNNELS_SPECIAL" in planetSpecials:
            if "TECTONIC_INSTABILITY_SPECIAL" not in planetSpecials:
                conditionalPopSizeMod += popSizeModMap["subHab"][planetEnv]
                if "TUNNELS_SPECIAL" in planetSpecials:
                    T_reason="Tunnels_PSM(%d)"
                else:
                    T_reason="Sub_Hab_PSM(%d)"
                detail.append(T_reason%popSizeModMap["subHab"][planetEnv])
        for gTech,  gKey in [ ("GRO_SYMBIOTIC_BIO", "symBio"),
                                                        ("GRO_XENO_GENETICS", "xenoGen"),
                                                        ("GRO_XENO_HYBRID", "xenoHyb"),
                                                        ("GRO_CYBORG", "cyborg") ]:
            if empire.getTechStatus(gTech) == fo.techStatus.complete:
                popSizeMod += popSizeModMap[gKey][planetEnv]
                detail.append("%s_PSM(%d)"%(gKey,  popSizeModMap[gKey][planetEnv]))
        for gTech,  gKey in [ ("CON_NDIM_STRUC", "ndim"),
                                                        ("CON_ORBITAL_HAB", "orbit") ]:
            if empire.getTechStatus(gTech) == fo.techStatus.complete:
                conditionalPopSizeMod += popSizeModMap[gKey][planetEnv]
                detail.append("%s_PSM(%d)"%(gKey,  popSizeModMap[gKey][planetEnv]))

        if "GAIA_SPECIAL" in planet.specials and species.name!="SP_EXOBOT": #exobots can't ever get to good environ so no gaiai bonus, for others we'll assume they'll get there
            conditionalPopSizeMod += 3
            detail.append("Gaia_PSM(3)")

        for special in [ "SLOW_ROTATION_SPECIAL",  "SOLID_CORE_SPECIAL"] :
            if special in planetSpecials:
                postPopSizeMod -= 1
                detail.append("%s_PSM(-1)"%special)

        applicableBoosts=set([])
        for thisTag in [ tag for tag in tagList if tag in  AIDependencies.metabolims]:
            metabBoosts= AIDependencies.metabolimBoostMap.get(thisTag,  [])
            if popSizeMod > 0:
                for key in activeGrowthSpecials.keys():
                    if  ( len(activeGrowthSpecials[key])>0 ) and ( key in metabBoosts ):
                        applicableBoosts.add(key)
                        detail.append("%s boost active"%key)
            for boost in metabBoosts:
                if boost in planetSpecials:
                    applicableBoosts.add(boost)
                    detail.append("%s boost present"%boost)

        nBoosts = len(applicableBoosts)
        if nBoosts:
            conditionalPopSizeMod += nBoosts
            detail.append("boosts_PSM(%d from %s)"%(nBoosts, applicableBoosts))
        if popSizeMod >= 0:
            popSizeMod += conditionalPopSizeMod
        popSizeMod += postPopSizeMod

        if planetID in species.homeworlds:  #TODO: check for homeworld growth focus
            popSizeMod+=2

        max_pop_size = planetSize * popSizeMod * popTagMod
        detail.append("baseMaxPop size*psm %d * %d * %.2f = %d"%(planetSize,  popSizeMod, popTagMod,   max_pop_size) )

        if "DIM_RIFT_MASTER_SPECIAL" in planet.specials:
            max_pop_size -= 4
            detail.append("DIM_RIFT_MASTER_SPECIAL(maxPop-4)")
        if "ECCENTRIC_ORBIT_SPECIAL" in planet.specials:
            max_pop_size -= 3
            detail.append("ECCENTRIC_ORBIT_SPECIAL(maxPop-3)")

        detail.append("maxPop %.1f"%max_pop_size)

        for special in [ "MINERALS_SPECIAL",  "CRYSTALS_SPECIAL",  "METALOIDS_SPECIAL"] :
            if special in planetSpecials:
                miningBonus+=1

        proSingVal = [0, 4][(len( claimedStars.get(fo.starType.blackHole,  [])) > 0)]
        basePopInd=0.2
        indMult=1 * max( indTagMod,  0.5*(indTagMod+resTagMod) ) #TODO: repport an actual calc for research value
        indTechMap={    "GRO_ENERGY_META":  0.5,
                                            "PRO_ROBOTIC_PROD":0.4,
                                            "PRO_FUSION_GEN":       1.0,
                                            "PRO_INDUSTRY_CENTER_I": 1,
                                            "PRO_INDUSTRY_CENTER_II":1,
                                            "PRO_INDUSTRY_CENTER_III":1,
                                            "PRO_SOL_ORB_GEN":  2.0,   #TODO don't assume will build a gen at a blue/white star
                                            "PRO_SINGULAR_GEN": proSingVal,
                                            }

        for tech in indTechMap:
            if (empire.getTechStatus(tech) == fo.techStatus.complete):
                indMult += indTechMap[tech]
        indVal = 0
        max_ind_factor = 0
        if (empire.getTechStatus("PRO_SENTIENT_AUTOMATION") == fo.techStatus.complete):
            fixedInd += discountMultiplier * 5
        if AIFocusType.FOCUS_INDUSTRY  in species.foci:
            #indVal += discountMultiplier * basePopInd * max_pop_size*miningBonus
            #indVal += discountMultiplier * basePopInd * max_pop_size * indMult
            max_ind_factor += basePopInd * miningBonus
            max_ind_factor += basePopInd * indMult
        cur_pop = 1.0 #assume an initial colonization vale 
        if planet.speciesName != "":
            cur_pop = planet.currentMeterValue(fo.meterType.population)
        elif  empire.getTechStatus("GRO_LIFECYCLE_MAN") == fo.techStatus.complete :
            cur_pop = 3.0 
        cur_industry = planet.currentMeterValue(fo.meterType.industry)
        indVal = project_ind_val(cur_pop,  max_pop_size,  cur_industry,  max_ind_factor, flat_industry, discountMultiplier)
        detail.append("indVal %.1f"%indVal)
        # used to give preference to closest worlds
        empireID = empire.empireID
        capitalID = PlanetUtilsAI.getCapital()
        homeworld = universe.getPlanet(capitalID)
        distanceFactor = 0
        if homeworld:
            homeSystemID = homeworld.systemID
            evalSystemID = this_sysid
            if (homeSystemID != -1) and (evalSystemID != -1):
                leastJumps = universe.jumpDistance(homeSystemID, evalSystemID)
                distanceFactor = 1.001 / (leastJumps + 1)

        for special in [ spec for spec in  planetSpecials if spec in AIDependencies.metabolimBoosts]:
            gbonus =  discountMultiplier  * basePopInd  * indMult * empireMetabolisms.get( AIDependencies.metabolimBoosts[special] , 0)#  due to growth applicability to other planets
            growthVal += gbonus
            detail.append( "Bonus for %s: %.1f"%(special,  gbonus))

        basePopRes = 0.2 #will also be doubling value of research, below
        if AIFocusType.FOCUS_RESEARCH in species.foci:
            researchBonus += discountMultiplier*2*basePopRes*max_pop_size
            if ( "ANCIENT_RUINS_SPECIAL" in planet.specials )  or ( "ANCIENT_RUINS_DEPLETED_SPECIAL" in planet.specials ):
                researchBonus += discountMultiplier*2*basePopRes*max_pop_size*5
                detail.append("Ruins Research")
            if "COMPUTRONIUM_SPECIAL" in planet.specials:
                researchBonus += discountMultiplier*2*10    #TODO: do actual calc
                detail.append("COMPUTRONIUM_SPECIAL")

        if max_pop_size <= 0:
            detail.append("Non-positive population projection for species '%s',  so no colonization value"%(species and species.name))
            return 0

        retval  += max(indVal+asteroidBonus+gasGiantBonus,  researchBonus,  growthVal)+fixedInd + fixedRes
        if this_sysid in annexableRing1:
            retval += 10
        elif this_sysid in annexableRing2:
            retval += 20
        elif this_sysid in annexableRing3:
            retval += 10

        retval *= priorityScaling

        if thrtFactor < 1.0:
            retval *=thrtFactor
            detail.append( "threat reducing value by %3d %%"%(100*(1-thrtFactor)))
        if haveExistingPresence:
            detail.append("preexisting system colony")
            retval *=1.5

    return retval

def removeLowValuePlanets(evaluatedPlanets):
    "removes all planets with a colonisation value < minimalColoniseValue"

    removeIDs = []
    minVal = AIstate.minimalColoniseValue
    if foAI.foAIstate.aggression <fo.aggression.typical:
        minVal *= 3

    # print ":: min:" + str(AIstate.minimalColoniseValue)
    for planetID in evaluatedPlanets.iterkeys():
        #print ":: eval:" + str(planetID) + " val:" + str(evaluatedPlanets[planetID])
        if (evaluatedPlanets[planetID][0] < minVal):
            removeIDs.append(planetID)
    #print "removing ",  removeIDs
    for ID in removeIDs: del evaluatedPlanets[ID]

def assignColonyFleetsToColonise():

    universe = fo.getUniverse()
    empire = fo.getEmpire()
    empireID = empire.empireID
    fleetSupplyableSystemIDs = empire.fleetSupplyableSystemIDs
    fleetSupplyablePlanetIDs = PlanetUtilsAI.getPlanetsInSystemsIDs(fleetSupplyableSystemIDs)

    allOutpostBaseFleetIDs = FleetUtilsAI.getEmpireFleetIDsByRole(AIFleetMissionType.FLEET_MISSION_ORBITAL_OUTPOST)
    availOutpostBaseFleetIDs = FleetUtilsAI.extractFleetIDsWithoutMissionTypes(allOutpostBaseFleetIDs)
    for fid in availOutpostBaseFleetIDs:
        fleet = universe.getFleet(fid)
        if not fleet: continue
        sysID = fleet.systemID
        system = universe.getSystem(sysID)
        availPlanets = set(system.planetIDs).intersection(set( foAI.foAIstate.qualifyingOutpostBaseTargets.keys()))
        targets = [pid for pid in availPlanets if foAI.foAIstate.qualifyingOutpostBaseTargets[pid][1] != -1 ]
        if not targets:
            print "Error found no valid target for outpost base in system %s (%d)"%(system.name,  sysID)
            continue

        targetID=-1
        bestScore=-1
        for pid,  rating in assignColonisationValues(targets, AIFleetMissionType.FLEET_MISSION_OUTPOST, fleetSupplyablePlanetIDs, None, empire).items():
            if rating[0]>bestScore:
                bestScore = rating[0]
                targetID = pid
        if targetID != -1:
            foAI.foAIstate.qualifyingOutpostBaseTargets[targetID][1] = -1 #TODO: should probably delete
            aiTarget = AITarget.AITarget(AITargetType.TARGET_PLANET, targetID)
            aiFleetMission = foAI.foAIstate.getAIFleetMission(fid)
            aiFleetMission.addAITarget(AIFleetMissionType.FLEET_MISSION_ORBITAL_OUTPOST, aiTarget)

    # assign fleet targets to colonisable planets
    sendColonyShips(AIstate.colonyFleetIDs, foAI.foAIstate.colonisablePlanetIDs, AIFleetMissionType.FLEET_MISSION_COLONISATION)

    # assign fleet targets to colonisable outposts
    sendColonyShips(AIstate.outpostFleetIDs, foAI.foAIstate.colonisableOutpostIDs, AIFleetMissionType.FLEET_MISSION_OUTPOST)


def sendColonyShips(colonyFleetIDs, evaluatedPlanets, missionType):
    "sends a list of colony ships to a list of planet_value_pairs"
    fleetPool = colonyFleetIDs[:]
    tryAll=False
    if   (missionType == AIFleetMissionType.FLEET_MISSION_OUTPOST ):
        cost = 20+AIDependencies.outpostPodCost * ( 1 + len(AIstate.popCtrIDs)*AIDependencies.colonyPodUpkeep )
    else:
        tryAll=True
        cost = 20+AIDependencies.colonyPodCost * ( 1 + len(AIstate.popCtrIDs)*AIDependencies.colonyPodUpkeep )
        if fo.currentTurn() < 50:
            cost *= 0.4  #will be making fast tech progress so value is underestimated
        elif fo.currentTurn() < 80:
            cost *= 0.8  #will be making fast-ish tech progress so value is underestimated

    potentialTargets =   [  (pid, (score, specName)  )  for (pid,  (score, specName) ) in  evaluatedPlanets if score > (0.8 * cost) ]

    print "colony/outpost  ship matching -- fleets  %s to planets %s"%( fleetPool,  evaluatedPlanets)

    if tryAll:
        print "trying best matches to current colony ships"
        bestScores= dict(evaluatedPlanets)
        potentialTargets = []
        for pid,  ratings in allColonyOpportunities.items():
            for rating in ratings:
                if rating[0] >= 0.75 * bestScores.get(pid,  [9999])[0]:
                    potentialTargets.append( (pid,  rating ) )
        potentialTargets.sort(lambda x, y: cmp(x[1], y[1]), reverse=True)

    #adding a lot of checking here because have been getting mysterious  exception, after too many recursions to get info
    fleetPool=set(fleetPool)
    universe=fo.getUniverse()
    empireID=fo.empireID()
    destroyedObjIDs = universe.destroyedObjectIDs(empireID)
    for fid in fleetPool:
        fleet = universe.getFleet(fid)
        if not fleet  or fleet.empty:
            print "Error: bad fleet ( ID %d ) given to colonization routine; will be skipped"%fid
            fleetPool.remove(fid)
            continue
        reportStr="Fleet ID (%d): %d ships; species: "%(fid,  fleet.numShips)
        for sid in fleet.shipIDs:
            ship = universe.getShip(sid)
            if not ship:
                reportStr += "NoShip, "
            else:
                reportStr += "%s,  "%ship.speciesName
        print reportStr
    print
    alreadyTargeted = []
    #for planetID_value_pair in evaluatedPlanets:
    while (len(fleetPool) > 0 ) and ( len(potentialTargets) >0):
        thisTarget = potentialTargets.pop(0)
        if thisTarget in alreadyTargeted:
            continue
        thisScore=thisTarget[1][0]
        thisPlanetID=thisTarget[0]
        if thisPlanetID in alreadyTargeted:
            continue
        thisPlanet = universe.getPlanet(thisPlanetID)
        #print "checking pool %s against target %s  current owner %s  targetSpec %s"%(fleetPool,  thisPlanet.name,  thisPlanet.owner,  thisTarget)
        thisSysID = thisPlanet.systemID
        if (foAI.foAIstate.systemStatus.setdefault(thisSysID, {}).setdefault('monsterThreat', 0) > 2000) or (fo.currentTurn() <20  and foAI.foAIstate.systemStatus[thisSysID]['monsterThreat'] > 200):
            print "Skipping colonization of system %s due to Big Monster,  threat %d"%(PlanetUtilsAI.sysNameIDs([thisSysID]),  foAI.foAIstate.systemStatus[thisSysID]['monsterThreat'])
            alreadyTargeted.append( thisPlanetID )
            continue
        thisSpec=thisTarget[1][1]
        foundFleets=[]
        try:
            thisFleetList = FleetUtilsAI.getFleetsForMission(nships=1,  targetStats={},  minStats={},  curStats={},  species=thisSpec,  systemsToCheck=[thisSysID],  systemsChecked=[],
                                                         fleetPoolSet = fleetPool,   fleetList=foundFleets,  triedFleets=set([]),  verbose=False)
        except:
            continue
        if thisFleetList==[]:
            fleetPool.update(foundFleets)#just to be safe
            continue #must have no compatible colony/outpost ships
        fleetID = thisFleetList[0]
        alreadyTargeted.append( thisPlanetID )

        aiTarget = AITarget.AITarget(AITargetType.TARGET_PLANET, thisPlanetID)
        aiFleetMission = foAI.foAIstate.getAIFleetMission(fleetID)
        aiFleetMission.addAITarget(missionType, aiTarget)
