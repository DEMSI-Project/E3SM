"""
"""

from CIME.XML.standard_module_setup import *
from CIME.utils import expect, run_cmd
from CIME.case import Case

import math

logger = logging.getLogger(__name__)

class TaskMaker(object):

    def __init__(self, case=None, remove_dead_tasks=False):
        self.case = case if case is not None else Case()
        self.remove_dead_tasks = remove_dead_tasks

        self.layout_strings = \
"""
COMP_CPL NTASKS_CPL NTHRDS_CPL ROOTPE_CPL PSTRID_CPL
COMP_ATM NTASKS_ATM NTHRDS_ATM ROOTPE_ATM PSTRID_ATM NINST_ATM
COMP_LND NTASKS_LND NTHRDS_LND ROOTPE_LND PSTRID_LND NINST_LND
COMP_ROF NTASKS_ROF NTHRDS_ROF ROOTPE_ROF PSTRID_ROF NINST_ROF
COMP_ICE NTASKS_ICE NTHRDS_ICE ROOTPE_ICE PSTRID_ICE NINST_ICE
COMP_OCN NTASKS_OCN NTHRDS_OCN ROOTPE_OCN PSTRID_OCN NINST_OCN
COMP_GLC NTASKS_GLC NTHRDS_GLC ROOTPE_GLC PSTRID_GLC NINST_GLC
COMP_WAV NTASKS_WAV NTHRDS_WAV ROOTPE_WAV PSTRID_WAV NINST_WAV
MAX_TASKS_PER_NODE PES_PER_NODE PIO_NUMTASKS PIO_ASYNC_INTERFACE
EXEROOT, COMPILER
""".split()

        for layout_string in self.layout_strings:
            setattr(self, layout_string, case.get_value(layout_string))

        self.DEFAULT_RUN_EXE_TEMPLATE_STR = "__DEFAULT_RUN_EXE__"
        self.MAX_TASKS_PER_NODE = 1 if self.MAX_TASKS_PER_NODE < 1 else self.MAX_TASKS_PER_NODE

        models = ["CPL", "ATM", "LND", "ROF", "ICE", "OCN", "GLC", "WAV"]
        for item in ["COMP", "NTASKS", "NTHRDS", "ROOTPE", "NINST", "PSTRID"]:
            values = []
            for model in models:
                if item == NINST and model == "CPL":
                    values.append(0) # no NINST for CPL, set to zero so lists are some length for all items
                else:
                    values.append(case.get_value("_".join([item, model])))

            setattr(self, item, values)

        self._compute_values()

    def _compute_values(self):
        total_tasks = 0
        for ntasks, rootpe, pstrid in zip(self.NTASKS, self.ROOTPE, self.PSTRID):
            tt = rootpe + (ntasks - 1) * pstrid + 1
            total_tasks = max(tt, total_tasks)

	# Check if we need to add pio's tasks to the total task count
        if self.PIO_ASYNC_INTERFACE:
            total_tasks += self.PIO_NUMTASKS if self.PIO_NUMTASKS > 0 else self.PES_PER_NODE

        # Compute max threads for each mpi task
        maxt = [0] * total_tasks
        for ntasks, nthrds, rootpe, pstrid in zip(self.NTASKS, self.NTHRDS, self.ROOTPE, self.PSTRID):
            c2 = 0
            while c2 < ntasks:
                s = rootpe + c2 * pstrid
                if nthrds > maxt[s]:
                    maxt[s] = nthrds

                c2 += 1

        # remove tasks with zero threads if requested
        if self.remove_dead_tasks:
            all_tasks = total_tasks
            for c1 in xrange(all_tasks):
                if c1 < total_tasks and maxt[c1] < 1:
                    for c3 in xrange(c1, total_tasks -1):
                        maxt[c3] = maxt[c3+1]

                    maxt[total_tasks] = 0
                    total_tasks -= 1

        # compute min/max threads over all mpi tasks and sum threads
	# reset maxt values from zero to one after checking for min values
	# but before checking for max and summing..
        min_threads = maxt[0]
        max_threads = maxt[0]

        self.sum_threads = [0]
        for c1 in xrange(1, total_tasks):
            if maxt[c1] < min_threads:
                min_threads = maxt[c1]
            if maxt[c1] < 1:
                maxt[c1] = 1
            if maxt[c1] > max_threads:
                max_threads = maxt[c1]

            self.sum_threads[c1] = self.sum_threads[c1-1] + maxt[c1-1]

        # Compute task and thread settings for batch commands
        full_sum = 0
        sum = maxt[0]
        task_geom = "(0"
        thread_geom = " %d" % maxt[0]
        task_count = 1
        max_task_count = total_tasks
        thread_count = maxt[0]
        max_thread_count = maxt[0]
        aprun = ""
        pbsrs = ""

        task_per_node, total_node_count, max_total_node_count = 0, 0, 0
        for c1 in xrange(1, total_tasks):
            sum += maxt[c1]

            if maxt[c1] > self.MAX_TASKS_PER_NODE:
                full_sum += self.MAX_TASKS_PER_NODE
                sum = maxt[c1]
                task_geom += ")(%d" % c1
            else:
                task_geom += ",%d" % c1

            thread_geom += ":%d" % maxt[c1]

            if maxt[c1] != thread_count:
                task_per_node = min(self.PES_PER_NODE, self.MAX_TASKS_PER_NODE / thread_count)

                task_per_node = min(task_count, task_per_node)

                aprun += " -n %d -N %d -d %d %s :" % (task_count, task_per_node, thread_count, self.DEFAULT_RUN_EXE_TEMPLATE_STR)

                node_count = int(math.ceil(float(task_count) / task_per_node))

                total_node_count += node_count
                pbsrs += "%d:ncpus=%d:mpiprocs=%d:ompthreads=%d:model=" % (node_count, self.MAX_TASKS_PER_NODE, task_per_node, thread_count)

                thread_count = maxt[c1]
                max_thread_count = max(max_thread_count, maxt[c1])
                task_count = 1

            else:
                task_count = 1

        max_task_per_node = min(self.MAX_TASKS_PER_NODE / max_thread_count, self.PES_PER_NODE, max_task_count)
        max_total_node_count = int(math.ceil(float(max_task_count) / max_task_per_node))

        full_sum += sum
        self.full_sum = full_sum
        task_geom += ")"
        self.task_geom = task_geom
        if self.PES_PER_NODE > 0:
            task_per_node = min(self.PES_PER_NODE, self.MAX_TASKS_PER_NODE / thread_count)
        else:
            task_per_node = self.MAX_TASKS_PER_NODE / thread_count
        task_per_node = min(task_count, task_per_node)

        total_node_count += int(math.ceil(float(task_count) / task_per_node))

        task_per_numa = int(math.ceil(task_per_node / 2.0))
        if self.COMPILER == "intel" and task_per_node > 1:
            aprun += " -S %d -cc numa_node " % task_per_numa

        aprun += " -n %d -N %d -d %d %s " % (task_count, task_per_node, thread_count, self.DEFAULT_RUN_EXE_TEMPLATE_STR)

	# add all the calculated numbers as instance data.
        self.total_tasks = total_tasks
        self.task_per_node = max_task_per_node
        self.task_per_numa = task_per_numa
        self.max_threads = max_threads
        self.min_threads = min_threads
        self.task_geom = task_geom
        self.thread_geom = thread_geom
        self.task_count = task_count
        self.thread_count = max_thread_count
        self.aprun = aprun
        self.opt_node_count = total_node_count
        self.node_count = max_total_node_count
        self.pbsrs = pbsrs + "%d:ncpus=%d:mpiprocs=%d:ompthreads=%d:model=" % (max_total_node_count, self.MAX_TASKS_PER_NODE, task_per_node, thread_count)

        # calculate ptile..
        ptile = self.MAX_TASKS_PER_NODE / 2
        if self.max_threads > 1:
            ptile = int(math.floor(float(self.MAX_TASKS_PER_NODE) / self.max_threads))

        self.ptile = ptile

    def has_opt_mpirun(self, mpirun_exe):
        """
        Has optimized mpirun command?
        """
        return hasattr(self, mpirun_exe)

    def opt_mpirun(self, mpirun_exe, model_exe):
        """
        Get optimized mpirun command
        """
        expect(has_opt_mpirun(mpirun_exe), "No opt mpirun")
        return getattr(self, mpirun_exe).replace(self.DEFAULT_RUN_EXE_TEMPLATE_STR, model_exe)

    def document(self):
        """
        Get the pe layout document for insertion into the run script
        """
        mydoc = \
"""
# ----------------------------------------
# PE Layout:
#   Total number of tasks: %d
#   Maximum threads per task: %d
""" % (self.total_tasks, self.max_threads)

        for comp, ntasks, nthrds, rootpe, in zip(self.COMP, self.NTASKS, self.NTHRDS, self.ROOTPE):
            doc += "#    %s ntasks=%d nthreads=%d rootpe=%d ninst=%d\n" % (comp, ntasks, nthrds, rootpe, ninst)

        doc += "#\n"
        $doc .=  "#    total number of hw pes = %d\n" % self.full_sum

        for comp, ntasks, nthrds, rootpe, pstrid in zip(self.COMP, self.NTASKS, self.NTHRDS, self.ROOTPE, self.PSTRID):
            tt = rootpe + (ntasks - 1) * pstrid
            tm = self.sum_threads[tt] + ntasks - 1
            doc += "#    %s hw pe range ~ from %d to %d\n" % (comp, self.sum_threads[rootpe], tm)

        if self.min_threads < 1:
            doc += \
"""#
#   WARNING there appear to be some IDLE hw pes
#   Please consider reviewing your env_mach_pes.xml file
"""

        doc += "# ----------------------------------------\n"

        return doc
