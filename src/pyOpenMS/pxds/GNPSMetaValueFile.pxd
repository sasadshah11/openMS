from Types cimport *
from String cimport *
from StringList cimport *

cdef extern from "<OpenMS/FORMAT/GNPSMetaValueFile.h>" namespace "OpenMS":

    cdef cppclass GNPSMetaValueFile:    

        GNPSMetaValueFile() nogil except +
        GNPSMetaValueFile(GNPSMetaValueFile &) nogil except +

        void store(StringList & mzml_file_paths, String & output_file) nogil except +
        # wrap-doc:
        #  Write meta value table (tsv file) from a list of mzML files. Required for GNPS FBMN.
        #  
        #  This will produce the minimal required meta values and can be extended manually.
        #  
        #  Parameters
        #  ----------
        #  mzml_file_paths: list of str
        #    Path list as string to the input mzML files.
        #  
        #  output_file : str
        #    Output file path for the meta value table.  
        