/*!
  \file aig_algebraic_rewriting.hpp
  \brief AIG algebraric rewriting

  EPFL CS-472 2021 Final Project Option 1
*/

#pragma once

#include "../networks/aig.hpp"
#include "../views/depth_view.hpp"
#include "../views/topo_view.hpp"

namespace mockturtle
{

namespace detail
{

template<class Ntk>
class aig_algebraic_rewriting_impl
{
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

public:
  aig_algebraic_rewriting_impl( Ntk& ntk )
      : ntk( ntk )
  {
    static_assert( has_level_v<Ntk>, "Ntk does not implement depth interface." );
  }

  void run()
  {
    bool cont{ true }; /* continue trying */
    while ( cont )
    {
      cont = false; /* break the loop if no updates can be made */
      ntk.foreach_gate( [&]( node n )
                        {
                          if ( try_algebraic_rules( n ) )
                          {
                            ntk.update_levels();
                            cont = true;
                          }
                        } );
    }
  }

private:
  /* Try various algebraic rules on node n. Return true if the network is updated. */
  bool try_algebraic_rules( node n )
  {
    if ( try_associativity( n ) )
      return true;
    if ( try_distributivity( n ) )
      return true;
    /* TODO: add more rules here... */
    if ( try_three_layer_distributivity( n ) )
      return true;

    return false;
  }

  bool try_associativity( node n )
  {
    bool flagAssociativity = false; 
    bool feasibility = false; // true if associativity is applicable

    uint32_t Level = 0;          // current level of node n
    uint32_t LevelBeta = 0;      // child level
    uint32_t betaSideLevel;      // child level not in the critical path
    uint32_t criticalGammaLevel; // grandchild level in the critical path
    uint32_t betaCounter = 0;    // counter of children

    signal betaSide, sideSignal; // child not in the critical path
    signal criticalBeta;         // child in the critical path

    if ( !ntk.is_on_critical_path( n ) ) // check if gate is in the critical path

      return false;

    else
    {

      Level = ntk.level( n );
      if ( Level == 1 ) // check if is PI

        return false;

      else
      {

        ntk.foreach_fanin( n, [&]( signal const& inSignal_n ) // cycling over node fanin
                           {
                             node nChild = ntk.get_node( inSignal_n ); // take gate which output is inSignal_n => child
                             if ( ntk.is_on_critical_path( nChild ) )  //check if gate child  is in the critical path

                             {
                               if ( !ntk.is_complemented( inSignal_n ) ) // check if signal fan in is boolean 0

                               {
                                 LevelBeta = ntk.level( nChild );
                                 if ( LevelBeta != 1 ) // child is not a PI

                                 {
                                   ntk.foreach_fanin( nChild, [&]( signal const& inSignalChild_n ) // iterate over child fan in
                                                      {
                                                        node nChild2 = ntk.get_node( inSignalChild_n ); //  take gate which output is inSignalchildn_n => granchild
                                                        if ( ntk.is_on_critical_path( nChild2 ) )       // check if grandchild is in the critical path
                                                        {

                                                          betaCounter++; // increase number of signal in inSignalChild_n in the fan in the critical path
                                                          // if it reach out 2 => both fan in are in the critical path thus no action is required
                                                          criticalBeta = inSignalChild_n;
                                                          criticalGammaLevel = ntk.level( nChild2 );
                                                        }
                                                        else

                                                          betaSide = inSignalChild_n; // child is not in the critical path

                                                        return;
                                                      } );
                                 }

                                 feasibility = true; // applicable if child is not PI and fan in are not complemented
                               }
                             }
                             else // children gate is  not in the CP

                             {

                               sideSignal = inSignal_n;
                               betaSideLevel = ntk.level( nChild );
                             }
                             return;
                           } );
      }
    }

    if ( feasibility && betaCounter == 1 )
    {

      if ( ( betaSideLevel <= criticalGammaLevel - 1 ) ) // verify that associativity is convenient : if side input
                                                         // of node n has a level lower than the node of child in the critical path
                                                         // create the network to substitute node n as reported in figure 1 (project option 1)
      {
        signal new_and_1th = ntk.create_and( sideSignal, betaSide );
        ntk.substitute_node( n, ntk.create_and( new_and_1th, criticalBeta ) );
        flagAssociativity = true;
      }
    }

    return flagAssociativity;
  }

  bool try_distributivity( node n )
  {
    bool flagDistributivity = false; 
    bool feasibility = false;   // true if associativity is applicable for node n
    bool betaFeasible = false;  // true if associativity is applicable for node child
    bool feasibleBeta1 = false; // true if associativity is applicable for node grandchildre
    uint32_t Level, LevelBeta;  // level of node n, level of child
    uint32_t counterPI = 0, betaCounter = 1;
    signal criticalBeta, betaSide, criticalBeta1, betaSide1;

    if ( !ntk.is_on_critical_path( n ) )

      return false;

    else

    {
      Level = ntk.level( n ); // check if gate is in the critical path
      if ( Level == 1 )

        return false; // check if is PI

      else
      {

        ntk.foreach_fanin( n, [&]( signal const& inSignal_n ) // cycling over node fanin
                           {
                             if ( ntk.is_complemented( inSignal_n ) ) // check if signal fan in is boolean 0
                             {

                               feasibility = true;                       // distributivity feasible
                               node nChild = ntk.get_node( inSignal_n ); // take gate which output is inSignal_n => child
                               LevelBeta = ntk.level( nChild );          // set children level

                               if ( LevelBeta == 1 ) // if child is on the first level => PI found

                                 counterPI++;

                               if ( counterPI != ntk.fanin_size( n ) ) // check if number of child node are directly connected to PI
                               {

                                 if ( ntk.is_on_critical_path( nChild ) ) // if child is on the criticls psth
                                 {

                                   ntk.foreach_fanin( nChild, [&]( signal const& inSignalChild_n ) // cycling over node fanin
                                                      {
                                                        node nChild2 = ntk.get_node( inSignalChild_n );                // take gate which output is inSignalChild_n => granchild
                                                        if ( !ntk.is_on_critical_path( nChild2 ) && betaCounter == 1 ) // check if granchild is not on the CP and there is one child on CP

                                                        {

                                                          betaFeasible = true; // node granchild is not on critical path
                                                          betaSide = inSignalChild_n;
                                                        }
                                                        else if ( ntk.is_on_critical_path( nChild2 ) && betaCounter == 1 ) // check if granchild is in the criticls path and child counter = 1

                                                          criticalBeta = inSignalChild_n; // signal granchild is in the CP

                                                        else if ( !ntk.is_on_critical_path( nChild2 ) )
                                                        {

                                                          feasibleBeta1 = true; // node granchild2 is not on CP
                                                          betaSide1 = inSignalChild_n;
                                                        }
                                                        else

                                                          criticalBeta1 = inSignalChild_n; // signal grandchild2  is in the CP

                                                        return;
                                                      } );

                                   betaCounter++; // used to determine if the number of child in the critical path are two.
                                                  // if is lower => distributivity in not convenient
                                 }
                               }
                             }

                             return;
                           } );
      }
    }

    if ( counterPI != ntk.fanin_size( n ) && feasibility && feasibleBeta1 && betaFeasible && criticalBeta1 == criticalBeta )
    // signal on critical path of child1 and child2 must be connected
    // create the network to substitute node n as reported in figure 3 (project option 1)
    {
      signal orNew = ntk.create_or( betaSide, betaSide1 );
      signal logicNew;
      if ( !ntk.is_or( n ) ) // check if output of node n is complemented or not

        logicNew = ntk.create_nand( orNew, criticalBeta );

      else

        logicNew = ntk.create_and( orNew, criticalBeta );

      ntk.substitute_node( n, logicNew );
      return true;
    }

    return false;
  }

  bool try_three_layer_distributivity( node n )
  {

    node nChild;
    bool critical = false;     // TRUE IF vector betalayer3 is updated
    bool non_critical = false; // TRUE IF vector otherBeta beta is updated

    std::vector<signal> betaLayer3, otherBeta; // vectors of signals

    if ( ntk.is_on_critical_path( n ) ) // gate n is in the criticsl path

    {
      ntk.foreach_fanin( n, [&]( signal const& inSignal_n )
                         {
                           node nChild = ntk.get_node( inSignal_n );
                           if ( ntk.is_on_critical_path( nChild ) && ntk.is_complemented( inSignal_n ) ) // boolean 0 and child is on the critical path

                           {
                             critical = true;
                             betaLayer3.push_back( inSignal_n );
                           }

                           else if ( !ntk.is_on_critical_path( nChild ) ) // child n is not in the critical path

                           {

                             otherBeta.push_back( inSignal_n );
                             non_critical = true;
                           }
                         } );

      if ( critical && non_critical )
      {

        critical = false;
        non_critical = false;
        ntk.foreach_fanin( ntk.get_node( betaLayer3.at( 0 ) ), [&]( signal const& inSignal_n )
                           {
                             node nChild = ntk.get_node( inSignal_n );
                             if ( ntk.is_on_critical_path( nChild ) && ntk.is_complemented( inSignal_n ) )

                             {

                               betaLayer3.push_back( inSignal_n );
                               critical = true;
                             }

                             else if ( !ntk.is_on_critical_path( nChild ) )

                             {

                               non_critical = true;
                               otherBeta.push_back( inSignal_n );
                             }
                           } );
      }

      else

        return false;

      if ( critical && non_critical )
      {

        critical = false;
        non_critical = false;
        ntk.foreach_fanin( ntk.get_node( betaLayer3.at( 1 ) ), [&]( signal const& inSignal_n )
                           {
                             nChild = ntk.get_node( inSignal_n );
                             if ( ntk.is_on_critical_path( nChild ) )

                             {

                               critical = true;
                               betaLayer3.push_back( inSignal_n );
                             }
                             else
                             {

                               otherBeta.push_back( inSignal_n );
                               non_critical = true;
                             }
                           } );
      }

      else

        return false;

      if ( critical && non_critical )

      {
        uint32_t level_three_layer = ntk.level( ntk.get_node( betaLayer3.at( 0 ) ) ), level_opt = ntk.level( ntk.get_node( otherBeta.at( 0 ) ) );
        if ( level_three_layer - 2 > level_opt )

        {
          signal AND_Bottom;
          AND_Bottom = ntk.create_and( otherBeta.at( 2 ), otherBeta.at( 0 ) );
          signal AND_Right;
          AND_Right = ntk.create_and( betaLayer3.at( 2 ), AND_Bottom );
          signal AND_Left;
          AND_Left = ntk.create_and( otherBeta.at( 0 ), ntk.create_not( otherBeta.at( 1 ) ) );
          signal new_netw = ntk.create_nand( ntk.create_not( AND_Right ), ntk.create_not( AND_Left ) );

          ntk.substitute_node( n, new_netw );

          return true;
        }
      }
    }

    return false;
  }

private:
  Ntk& ntk;
};
} // namespace detail
template<class Ntk>
void aig_algebraic_rewriting( Ntk& ntk )
{
  static_assert( std::is_same_v<typename Ntk::base_type, aig_network>, "Ntk is not an AIG" );

  depth_view dntk{ ntk };
  detail::aig_algebraic_rewriting_impl p( dntk );
  p.run();
}
} // namespace mockturtle
