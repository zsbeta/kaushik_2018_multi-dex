#!/usr/bin/env python
# encoding: utf-8
#| Copyright Inria May 2015
#| This project has received funding from the European Research Council (ERC) under
#| the European Union's Horizon 2020 research and innovation programme (grant
#| agreement No 637972) - see http://www.resibots.eu
#|
#| Contributor(s):
#|   - Jean-Baptiste Mouret (jean-baptiste.mouret@inria.fr)
#|   - Antoine Cully (antoinecully@gmail.com)
#|   - Kontantinos Chatzilygeroudis (konstantinos.chatzilygeroudis@inria.fr)
#|   - Federico Allocati (fede.allocati@gmail.com)
#|   - Vaios Papaspyros (b.papaspyros@gmail.com)
#|
#| This software is a computer library whose purpose is to optimize continuous,
#| black-box functions. It mainly implements Gaussian processes and Bayesian
#| optimization.
#| Main repository: http://github.com/resibots/limbo
#| Documentation: http://www.resibots.eu/limbo
#|
#| This software is governed by the CeCILL-C license under French law and
#| abiding by the rules of distribution of free software.  You can  use,
#| modify and/ or redistribute the software under the terms of the CeCILL-C
#| license as circulated by CEA, CNRS and INRIA at the following URL
#| "http://www.cecill.info".
#|
#| As a counterpart to the access to the source code and  rights to copy,
#| modify and redistribute granted by the license, users are provided only
#| with a limited warranty  and the software's author,  the holder of the
#| economic rights,  and the successive licensors  have only  limited
#| liability.
#|
#| In this respect, the user's attention is drawn to the risks associated
#| with loading,  using,  modifying and/or developing or reproducing the
#| software by the user in light of its specific status of free software,
#| that may mean  that it is complicated to manipulate,  and  that  also
#| therefore means  that it is reserved for developers  and  experienced
#| professionals having in-depth computer knowledge. Users are therefore
#| encouraged to load and test the software's suitability as regards their
#| requirements in conditions enabling the security of their systems and/or
#| data to be ensured and,  more generally, to use and operate it in the
#| same conditions as regards security.
#|
#| The fact that you are presently reading this means that you have had
#| knowledge of the CeCILL-C license and that you accept its terms.
#|
#! /usr/bin/env python
# Konstantinos Chatzilygeroudis - 2015

"""
Quick n dirty KDL detection
"""

import os, glob, types
from waflib.Configure import conf


def options(opt):
    opt.add_option('--kdl', type='string', help='path to kdl', dest='kdl')


@conf
def check_kdl(conf):
    if conf.options.kdl:
        includes_check = [conf.options.kdl + '/include']
        libs_check = [conf.options.kdl + '/lib']
    else:
        includes_check = ['/usr/local/include', '/usr/include']
        libs_check = ['/usr/local/lib', '/usr/lib', '/usr/lib/x86_64-linux-gnu/']
        if 'ROS_DISTRO' in os.environ:
            includes_check = ['/opt/ros/' + os.environ['ROS_DISTRO'] + '/include'] + includes_check
            libs_check = ['/opt/ros/' + os.environ['ROS_DISTRO'] + '/lib'] + libs_check

    files = ['kdl/chainfksolverpos_recursive.hpp', 'kdl/chainiksolvervel_pinv.hpp', 'kdl/chain.hpp', 'kdl/tree.hpp', 'urdf_model/model.h']

    conf.start_msg('Checking for KDL includes')
    for f in files:
        try:
            res = conf.find_file(f, includes_check)
        except:
            conf.end_msg('Not found', 'RED')
            return 1
    conf.end_msg('ok')

    conf.start_msg('Checking for KDL libs')
    for lib in ['liborocos-kdl.so', 'liburdfdom_model.so']:
        try:
            res = conf.find_file(lib, libs_check)
        except:
            conf.end_msg('Not found', 'RED')
            return 1
    conf.end_msg('ok')
    conf.env.INCLUDES_KDL = includes_check
    conf.env.LIBPATH_KDL = libs_check
    conf.env.DEFINES_KDL = ['USE_KDL']
    conf.env.LIB_KDL = ['orocos-kdl', 'urdfdom_model']
    return 1
