"""
Simple interface to the Globus GridFTP client API.
"""
import sys
import exceptions
import types
import gridftpwrapper

class GridFTPClientException(exceptions.Exception):
    """
    A simple subclass of Exception.

    Used for errors in the gridftpClient module.
    """
    pass

class HandleAttr(object):
    """
    A wrapping of the Globus GridFTP API globus_ftp_client_handleattr_t.
    """
    def __init__(self):
        """
        Constructs an instance. A wrapped pointer to the Globus C type
        that is created is stored as the ._attr attribute to the 
        instance.

        @rtype: instance
        @return: an instance of the class

        @raise GridFTPClientException: raised if unable to initialize
        the Globus C type
        """

        self._attr = None

        try:
            self._attr = gridftpwrapper.gridftp_handleattr_init();
        except Exception, e:
            msg = "Unable to initialize a handle attr: %s" % e
            ex = GridFTPClientException(msg)
            raise ex

    def destroy(self):
        """
        Destroy an instance. The wrapped pointer to the Globus C type
        is used by globus_free() to free all the memory associated
        with the Globus C type.

        @rtype: None
        @return: None

        @raise GridFTPClientException: raised if unable to free the
        memory associated with the Globus C type
        """

        if self._attr:
            try:
                gridftpwrapper.gridftp_handleattr_destroy(self._attr)
            except Exception, e:
                msg = "Unable to destroy a handle attr: %s" % e
                ex = GridFTPClientException(msg)
                raise ex

    def set_cache_all(self):
        """
        Set the cache all connections attribute for an ftp
        client handle attribute set.

        This attribute allows the user to cause all control
        connections to be cached between ftp operations.
        When this is enabled, the user skips the
        authentication handshake and connection
        establishment overhead for multiple subsequent ftp
        operations to the same server.

        @rtype: None
        @return: None

        @raise GridFTPClientException: raised if unable to
        set cache all for the handle attribute.

        """
        try:
            gridftpwrapper.gridftp_handleattr_set_cache_all(self._attr, 1)
        except Exception, e:
            msg = "Unable to set cache all for handle attr: %s" % e
            ex = GridFTPClientException(msg)
            raise ex

class OperationAttr(object):
    """
    A wrapping of the Globus GridFTP API globus_ftp_client_operationattr_t.
    """
    def __init__(self):
        """
        Constructs an instance. A wrapped pointer to the Globus C type
        that is created is stored as the ._attr attribute to the 
        instance.

        @rtype: instance
        @return: an instance of the class

        @raise GridFTPClientException: raised if unable to initialize
        the Globus C type
        """

        self._attr = None

        try:
            self._attr = gridftpwrapper.gridftp_operationattr_init();
        except Exception, e:
            msg = "Unable to initialize an operation attr: %s" % e
            ex = GridFTPClientException(msg)
            raise ex

    def destroy(self):
        """
        Destroy an instance. The wrapped pointer to the Globus C type
        is used by globus_free() to free all the memory associated
        with the Globus C type.

        @rtype: None
        @return: None

        @raise GridFTPClientException: raised if unable to free the
        memory associated with the Globus C type
        """

        if self._attr:
            try:
                gridftpwrapper.gridftp_operationattr_destroy(self._attr)
            except Exception, e:
                msg = "Unable to destroy an operation attr: %s" % e
                ex = GridFTPClientException(msg)
                raise ex

    def set_mode_extended_block(self):
        """
        Set the file transfer mode attribute for an ftp
        client attribute set to EXTENDED BLOCK MODE.

        Extended block mode is a file transfer mode where
        data can be sent over multiple parallel connections
        and to multiple data storage nodes to provide a
        high-performance data transfer. In extended block
        mode, data may arrive out-of-order. ASCII type files
        are not supported in extended block mode.

        @rtype: None
        @return: None

        @raise GridFTPClientException: raised if unable to
        set cache all for the handle attribute.

        """
        mode = gridftpwrapper.GLOBUS_FTP_CONTROL_MODE_EXTENDED_BLOCK
        try:
            gridftpwrapper.gridftp_operationattr_set_mode(self._attr, mode)
        except Exception, e:
            msg = "Unable to set mode to extended block on operation attr: %s" % e
            ex = GridFTPClientException(msg)
            raise ex

    def set_parallelism(self, parallelism):
        """
        Set the parallelism attribute for an ftp client
        attribute set.

        This attribute allows the user to control the level
        of parallelism to be used on an extended block mode
        file transfer. Currently, only a "fixed" parallelism
        level is supported. This is interpreted by the FTP
        server as the number of parallel data connections to
        be allowed for each stripe of data. 

        To set the number of parallel channels to be used
        for a transfer create an instance of the Parallelism
        class and set the mode and size and then use that
        instance as the argument to this method.

        @param parallelism: An instance of the Parallelism
        class prepared with the correct mode and size to 
        represent the number of parallel channels to use.
        @type parallelism: instance of Parallelism

        @return: None
        @rtype: None

        @raise GridFTPClientException: raised if unable to
        set the parallelism
        """
        if not isinstance(parallelism, Parallelism):
            msg = "Argument must be an instance of class Parallelism"
            ex = GridFTPClientException(msg)
            raise ex

        try:
            gridftpwrapper.gridftp_operationattr_set_parallelism(self._attr, parallelism._parallelism)
        except Exception, e:
            msg = "Unable to set parallelism on operation attr: %s" % e
            ex = GridFTPClientException(msg)
            raise ex

    def set_tcp_buffer(self, tcpbuffer):
        """
        Set the TCP buffer attribute for an ftp client attribute set.

        This attribute allows the user to control the TCP
        buffer size used for all data channels used in a
        file transfer.

        To set the TCP buffer size for a transfer create an
        instance of the TCPBuffer class and set the mode and
        size and then use that instance as the argument to 
        this method.

        @param tcpbuffer: An instance of the TcpBuffer class
        prepared with the correct mode and size to represent
        the side of the TCP buffer that should be used for
        the transfer.
        @type tcpbuffer: instance of TCPBuffer

        @return: None
        @rtype: None

        @raise GridFTPClientException: raised if unable to
        set the parallelism

        """
        if not isinstance(tcpbuffer, TcpBuffer):
            msg = "Argument must be an instance of class TcpBuffer"
            ex = GridFTPClientException(msg)
            raise ex

        try:
            gridftpwrapper.gridftp_operationattr_set_tcp_buffer(self._attr, tcpbuffer._tcpbuffer)
        except Exception, e:
            msg = "Unable to set tcpbuffer on operation attr: %s" % e
            ex = GridFTPClientException(msg)
            raise ex

class Parallelism(object):
    """
    A wrapping of the Globus GridFTP API globus_ftp_control_parallelism_t.
    """
    def __init__(self):
        """
        Constructs an instance. A wrapped pointer to the Globus C type
        that is created is stored as the ._parallelism attribute to the 
        instance.

        @rtype: instance
        @return: an instance of the class

        @raise GridFTPClientException: raised if unable to initialize
        the Globus C type
        """
        self._parallelism = None

        try:
            self._parallelism = gridftpwrapper.gridftp_parallelism_init()
        except Exception, e:
            msg = "Unable to create parallelism object: %s" % e
            ex = GridFTPClientException(msg)
            raise ex

    def destroy(self):
        """
        Destroy an instance. The wrapped pointer to the Globus C type
        is used by globus_free() to free all the memory associated
        with the Globus C type.

        @rtype: None
        @return: None

        @raise GridFTPClientException: raised if unable to free the
        memory associated with the Globus C type
        """
        if self._parallelism:
            try:
                gridftpwrapper.gridftp_parallelism_destroy(self._parallelism)
            except Exception, e:
                msg = "Unable to destroy parallelism object: %s" % e
                ex = GridFTPClientException(msg)
                raise ex

    def set_mode_fixed(self):
        """
        Sets the mode of parallelism to "fixed" or
        GLOBUS_FTP_CONTROL_PARALLELISM_FIXED, which is currently the only
        mode supported.

        @rtype: None
        @return: None

        @raise GridFTPClientException: raised if unable to set the mode to
        fixed.
        """
        try:
            mode = gridftpwrapper.GLOBUS_FTP_CONTROL_PARALLELISM_FIXED
            gridftpwrapper.gridftp_parallelism_set_mode(self._parallelism, mode)
        except Exception, e:
            msg = "Unable to set mode to fixed: %s" % e
            ex = GridFTPClientException(msg)
            raise ex

    def set_size(self, size):
        """
        Sets the number of parallel data connections to be used.

        @param size: the number of parallel data connections
        @type size: integer

        @rtype: None
        @return: None

        @raise GridFTPClientException: raised if unable to set the number
        of parallel data connections
        """
        try:
            gridftpwrapper.gridftp_parallelism_set_size(self._parallelism, size)
        except Exception, e:
            msg = "Unable to set size to %d for parallel data streams: %s" % (size, e)
            ex = GridFTPClientException(msg)
            raise ex

class TcpBuffer(object):
    """
    A wrapping of the Globus GridFTP API globus_ftp_control_tcpbuffer_t.
    """
    def __init__(self):
        """
        Constructs an instance. A wrapped pointer to the Globus C type
        that is created is stored as the ._tcpbuffer attribute to the 
        instance.

        @rtype: instance
        @return: an instance of the class

        @raise GridFTPClientException: raised if unable to initialize
        the Globus C type
        """
        self._tcpbuffer = None

        try:
            self._tcpbuffer = gridftpwrapper.gridftp_tcpbuffer_init()
        except Exception, e:
            msg = "Unable to create tcpbuffer object: %s" % e
            ex = GridFTPClientException(msg)
            raise ex

    def destroy(self):
        """
        Destroy an instance. The wrapped pointer to the Globus C type
        is used by globus_free() to free all the memory associated
        with the Globus C type.

        @rtype: None
        @return: None

        @raise GridFTPClientException: raised if unable to free the
        memory associated with the Globus C type
        """
        if self._tcpbuffer:
            try:
                gridftpwrapper.gridftp_tcpbuffer_destroy(self._tcpbuffer)
            except Exception, e:
                msg = "Unable to destroy tcpbuffer object: %s" % e
                ex = GridFTPClientException(msg)
                raise ex

    def set_mode_fixed(self):
        """
        Sets the mode of the tcp buffer to "fixed" or
        GLOBUS_FTP_CONTROL_TCPBUFFER_FIXED.

        @rtype: None
        @return: None

        @raise GridFTPClientException: raised if unable to set the mode to
        fixed.
        """
        try:
            mode = gridftpwrapper.GLOBUS_FTP_CONTROL_TCPBUFFER_FIXED
            gridftpwrapper.gridftp_tcpbuffer_set_mode(self._tcpbuffer, mode)
        except Exception, e:
            msg = "Unable to set mode to fixed: %s" % e
            ex = GridFTPClientException(msg)
            raise ex


    def set_size(self, size):
        """
        Sets the size of the tcp buffer to be used for the data connections.

        @param size: the size in bytes for the tcp buffer
        @type size: integer

        @rtype: None
        @return: None

        @raise GridFTPClientException: raised if unable to set the number
        of parallel data connections
        """
        try:
            gridftpwrapper.gridftp_tcpbuffer_set_size(self._tcpbuffer, size)
        except Exception, e:
            msg = "Unable to set size to %d for tcpbuffer: %s" % (size, e)
            ex = GridFTPClientException(msg)
            raise ex


class Buffer(object):
    """
    A wrapping of the Globus API globus_byte_t.
    """
    def __init__(self, size):
        """
        Constructs an instance. A wrapped pointer to the Globus C type
        that is created is stored as the ._buffer attribute to the 
        instance.

        @rtype: instance
        @return: an instance of the class

        @raise GridFTPClientException: raised if unable to initialize
        the Globus C type
        """
        self._buffer = None

        try:
            self._buffer = gridftpwrapper.gridftp_create_buffer(size)
        except Exception, e:
            msg = "Unable to create buffer: %s" % e
            ex = GridFTPClientException(msg)
            raise ex

        self.size = size

    def destroy(self):
        """
        Destroy an instance. The wrapped pointer to the Globus C type
        is used by globus_free() to free all the memory associated
        with the Globus C type.

        @rtype: None
        @return: None

        @raise GridFTPClientException: raised if unable to free the
        memory associated with the Globus C type
        """
        if self._buffer:
            try:
                gridftpwrapper.gridftp_destroy_buffer(self._buffer)
            except Exception, e:
                msg = "Unable to destroy buffer: %s" % e
                ex = GridFTPClientException(msg)
                raise ex

    def as_string(self, size):
        """
        Return the contents of a buffer as a Python string of length size.

        @param size: the number of bytes to return
        @type size: integer

        @return: the contents of the buffer as a Python string
        @rtype: string

        @raise GridFTPClientException: raised if unable to convert the
        contents of the buffer as a string
        """
        if self._buffer:
            try:
                self.string = gridftpwrapper.gridftp_buffer_to_string(self._buffer, size)
            except Exception, e:
                msg = "Unable to convert buffer to string: %s" % e
                ex = GridFTPClientException(msg)
                raise ex
                
            return self.string    

class PerformanceMarkerPlugin(object):
    """
    A wrapping of the Globus GridFTP API globus_ftp_client_plugin_t for use
    with the performance marker plugin.
    """
    def __init__(self, beginCB, markerCB, completeCB, arg):
        """
        Constructs an instance. A wrapped pointer to the Globus C type
        that is created is stored as the ._plugin attribute to the 
        instance. A wrapper pointer to the C struct used to hold
        pointers to the Python callback functions is also stored 
        as the ._callback attribute.

        The callbacks must have the following structure:

        beginCB
        =======
            def callback(arg, handle, src, dst, restart):
                - arg: the user argument passed in when plugin is created
                - handle: the wrapped pointer to the client handle
                - src: the source URL for the transfer
                - dst: the destination URL for the transfer
                - restart: an integer indicating if the transfer has been
                  restarted or not

        markerCB
        ========
            def callback(arg, handle, timestamp, timestamp_tenth, stripe_index, num_stripes, nbytes):
                - arg: the user argument passed in when plugin is created
                - handle: the wrapped pointer to the client handle
                - timestamp: integer indicating at which time the nbytes
                  argument is valid
                - timestamp_tenth: integer indicating the tenth place for
                  the time at which nbytes argument is valid
                - stripe_index: the stripe for which the marker is valid
                  (note that stripe is not same as parallel connection)
                - num_stripes: the number of stripes for this transfer
                - nbytes: the number of bytes transferred so far

        completeCB
        ==========
            def callback(arg, handle, success):
                - arg: the user argument passed in when plugin is created
                - handle: the wrapped pointer to the client handle
                - success: integer indicating if the transfer completed
                  successfully with 0 indicating failure

        @rtype: instance
        @return: an instance of the class

        @param beginCB: the Python function to call when a transfer begins
        @type beginCB: callable

        @param markerCB: the Python function to call when a performance
        marker is received
        @type markerCB: callable

        @param completeCB: the Python function to call when a transfer
        completes
        @type completeCB: callable

        @raise GridFTPClientException: raised if unable to initialize
        the Globus C type
        """

        self._plugin = None
        self._callback = None

        try:
            self._plugin, self._callback = gridftpwrapper.gridftp_perf_plugin_init(beginCB, markerCB, completeCB, arg)
        except Exception, e:
            msg = "Unable to initialize perf plugin: %s" % e
            ex = GridFTPClientException(msg)
            raise ex

    def destroy(self):
        """
        Destroy an instance. The wrapped pointer to the Globus C type
        is used by globus_free() to free all the memory associated
        with the Globus C type. The wrapped pointer to the C struct used to
        hold pointers to the callback functions is also freed.

        @rtype: None
        @return: None

        @raise GridFTPClientException: raised if unable to free the
        memory associated with the Globus C type
        """

        if self._plugin and self._callback:
            try:
                gridftpwrapper.gridftp_perf_plugin_destroy(self._plugin, self._callback)
            except Exception, e:
                msg = "Unable to destroy perf plugin: %s" % e
                ex = GridFTPClientException(msg)


class FTPClient(object):
    """
    A class to wrap the GridFTP client functions
    """

    def __init__(self, handleAttr):
        """
        Constructs an instance. A wrapped pointer to the Globus C type 
        globus_ftp_client_handle_t is stored in the ._handle attribute.

        @param handleAttr: instance of the HandleAttr class
        @type handleAttr: instance of HandleAttr class

        @return: an instance of class GridFTPClient
        @rtype: instance of class GridFTPClient

        @raise GridFTPClientException:  thrown if unable to init the module
        or open the connection to the server.
        """

        self._handleAttr = handleAttr
        self._handle = None

        # create a handle for this client
        try:
            self._handle = gridftpwrapper.gridftp_handle_init(self._handleAttr._attr)
        except Exception, e:
            msg = "Unable to create a handle: %s" % e
            ex = GridFTPClientException(msg)
            raise ex

    def destroy(self):
        """
        Destroys an instance. The wrapped pointer to the Globus C type is
        used by globus_free() to free all the memory associated with the C
        type.

        @return: None
        @rtype: None
        
        @raise GridFTPClientException: thrown if unable to destroy the
        instance
        """

        if self._handle:
            try: 
                gridftpwrapper.gridftp_handle_destroy(self._handle)
            except Exception, e:
                msg = "Unable to destroy client handle: %s" % e
                ex = GridFTPClientException(msg)
                raise ex

    def add_plugin(self, plugin):
        """
        Add a plugin to the handle associated with this instance.

        @param plugin: an instance of a plugin class, currently only the
        PerformanceMarkerPlugin class is supported
        @type plugin: instance of PerformanceMarkerPlugin

        @return: None
        @rtype: None

        @raise GridFTPClientException: thrown if unable to add the plugin
        to the handle
        """
        if self._handle:
            try:
                gridftpwrapper.gridftp_handle_add_plugin(self._handle, plugin._plugin)
            except Exception, e:
                msg = "Unable to add plugin: %s" % e
                ex = GridFTPClientException(msg)
                raise ex

    def remove_plugin(self, plugin):
        """
        Remove a plugin from the handle associated with this instance. The
        plugin must have already been added using the add_plugin() method.

        @param plugin: an instance of a plugin class, currently only the
        PerformanceMarkerPlugin class is supported
        @type plugin: instance of PerformanceMarkerPlugin

        @return: None
        @rtype: None

        @raise GridFTPClientException: thrown if unable to remove the plugin
        to the handle
        """
        if self._handle:
            try:
                gridftpwrapper.gridftp_handle_remove_plugin(self._handle, plugin._plugin)
            except Exception, e:
                msg = "Unable to remove plugin: %s" % e
                ex = GridFTPClientException(msg)
                raise ex

    def third_party_transfer(self, src, dst, completeCallback, arg, 
                srcOpAttr = None, dstOpAttr = None, restartMarker = None):
        """
        Initiate a third party transfer between to servers. This function
        returns immediately.  When the transfer is completed or if the
        transfer is aborted, the completeCallback function will be invoked with the
        final status of the transfer.

        The completeCallback must have the form:

        def completeCallback(arg, handle, error):
            - arg is the user argument passed in when the transfer was
              initiated
            - handle is the wrapped pointer to the client handle
            - error is None for success or a string if an error occurred

        @param src: the source URL for the transfer
        @type src: string

        @param dst: the destination URL for the transfer
        @type dst: string

        @param completeCallback: the function to call when the transfer is
        complete
        @type completeCallback: callable

        @param arg: user argument to pass to the completion callback 
        @type arg: any

        @param srcOpAttr: an instance of OperationAttr for the source
        @type srcOpAttr: instance of OperationAttr

        @param dstOpAttr: an instance of OperationAttr for the destination
        @type dstOpAttr: instance of OperationAttr

        @param restartMarker: not currently supported, please pass in None
        @type restartMarker: None

        @return: None
        @rtype: None

        @raise GridFTPClientException: raised if unable to initiate the
        third party transfer
        """

        if not srcOpAttr or not dstOpAttr:
            msg = "Both a source and destination OperationAttr instance must be input"
            ex = GridFTPClientException(msg)
            raise ex

        try:
            gridftpwrapper.gridftp_third_party_transfer(
                self._handle,
                src,
                srcOpAttr._attr,
                dst,
                dstOpAttr._attr,
                None,
                completeCallback,
                arg
                )
        except Exception, e:
            msg = "Unable to initiate third party transfer: %s" % e
            ex = GridFTPClientException(msg)
            raise ex

    def get(self, url, completeCallback, arg, opAttr = None, marker = None):
        """
        Get a file from an FTP server.

        This function starts a get file transfer from an FTP server. If
        this function returns without exception then the user may immediately
        begin calling register_read() to retrieve the data associated with this URL.

        When all of the data associated with this URL is retrieved, and all
        of the data callbacks have been called, or if the get request is
        aborted, the completeCallback will be invoked with the final
        status of the get.

        The completeCallback function must have the form:

        def completeCallback(arg, handle, error):
            - arg is the user argument passed in when the transfer was
              initiated
            - handle is the wrapped pointer to the client handle
            - error is None for success or a string if an error occurred

        @param url: the source URL to get
        @type url: string

        @param completeCallback: function to call when the transfer is
        complete
        @type completeCallback: callable

        @param arg: user argument to pass to the callback
        @type arg: any

        @param opAttr: an instance of OperationAttr for the transfer
        @type opAttr: instance of OperationAttr

        @param marker: not currently supported, please pass in None
        @type marker: None

        @return: None
        @rtype: None

        @raise GridFTPClientException: raised if unable to initaite the get
        operation

        """
        if not opAttr:
            msg = "An OperationAttr instance must be input"
            ex = GridFTPClientException(msg)
            raise ex

        try:
            gridftpwrapper.gridftp_get(
                self._handle, 
                url,
                opAttr._attr,
                None,
                completeCallback,
                arg
                )
        except Exception, e:
            msg = "Unable to initiate get: %s" % e
            ex = GridFTPClientException(msg)
            raise ex

    def register_read(self, buffer, dataCallback, arg):
        """
        Register an instance of class Buffer and the function dataCallback
        to handle a part of the FTP data transfer.

        The instance of class Buffer will be associated with the current get being
        performed on this client handle and data will be written into the
        buffer. The user must then empty the buffer and register another
        instance for the transfer to proceed. Note that if parallel data
        channels are used multiple buffers should be registered.

        When the instance of class Buffer is fulled the function
        dataCallback will be called.

        The dataCallback function must have the form:

        def dataCallback(arg, handle, buffer, length, offset, eof, error):
            - arg is the user argument passed in when this function is called
            - handle is the wrapped pointer to the client handle
            - buffer is the buffer from which the data can be read
            - length is the number of bytes in the buffer
            - offset is the offset into the file at which the bytes start
            - eof is true if this is the end of the file
            - error is None or a string if there is an error

        @param buffer: instance of class Buffer into which the data will be
        written
        @type buffer: instance of class Buffer

        @param dataCallback: function to be called when the instance of
        Buffer is full
        @type dataCallback: callable

        @param arg: user argument to pass to the callback function
        @type arg: any

        @return: None
        @rtype: None

        @raises GridFTPException: raised if unable to register the buffer
        and callback for reading

        """
        try:
            gridftpwrapper.gridftp_register_read(
                self._handle,
                buffer._buffer,
                buffer.size,
                dataCallback,
                arg
                )
        except Exception, e:
            msg = "Unable to register read: %s" % e
            ex = GridFTPClientException(msg)
            raise ex

            
    def cksm(self, url, completeCallback, arg, opAttr = None, offset = None, length = None):
        """
        Get a file's checksum from an FTP server.

        This function requests the checksum of a file from an FTP server.

        When the request is completed or aborted, the completeCallback
        will be invoked with the final status of the operation and the
        checksum value. 

        The completeCallback must have the form:

        def completeCallback(cksm, arg, handle, error):
            - cksm is the Python string containing the checksum value
            - arg is the user argument passed in when the call was
              initiated
            - handle is the wrapper pointer to the client handle
            - error is None for success or a string if an error occurred

        @param url: the source URL for the file to be checksummed
        @type url: string

        @param completeCallback: the function to be called when the checksum
        operation is complete
        @type completeCallback: callable

        @param arg: user argument to pass to the callback function
        @type arg: any

        @param opAttr: an instance of OperationAttr for the source
        @type opAttr: instance of OperationAttr

        @param offset: the offset in bytes into the file at which to begin computing
        the checkusm, use None to start at the beginning of the file
        @type offset: integer

        @param length: the length of the file in bytes to use when
        computing the checksum, use None to checksum the entire file
        @type length: integer

        @return: None
        @rtype: None

        @raise GridFTPClientException: raised if unable to initiate the
        checksum operation
        """

        if not opAttr:
            msg = "An OperationAttr instance must be input"
            ex = GridFTPClientException(msg)
            raise ex
        if not offset:
            offset = 0
        if not length:
            length = -1

        try:
            gridftpwrapper.gridftp_cksm(self._handle, url, opAttr._attr, offset, length, completeCallback, arg)
        except Exception, e:
            msg = "Unable to cksm: %s" % e
            ex = GridFTPClientException(msg)
            raise ex

    def mkdir(self, url, completeCallback, arg, opAttr = None):
        """
        Make a directory on a server.

        When the request is completed or aborted, the completeCallback
        will be invoked with the final status of the
        operation.

        The completeCallback must have the form:

        def completeCallback(arg, handle, error):
            - arg is the user argument passed in when the call was
              initiated
            - handle is the wrapper pointer to the client handle
            - error is None for success or a string if an error occurred

        @param url: the source URL for the directory to be created
        @type url: string

        @param completeCallback: the function to be called
        when the mkdir operation is complete
        @type completeCallback: callable

        @param arg: user argument to pass to the callback function
        @type arg: any

        @param opAttr: an instance of OperationAttr for the source
        @type opAttr: instance of OperationAttr

        @return: None
        @rtype: None

        @raise GridFTPClientException: raised if unable to initiate the
        mkdir operation
        """

        if not opAttr:
            msg = "An OperationAttr instance must be input"
            ex = GridFTPClientException(msg)
            raise ex

        try:
            gridftpwrapper.gridftp_mkdir(self._handle, url, opAttr._attr, completeCallback, arg)
        except Exception, e:
            msg = "Unable to mkdir: %s" % e
            ex = GridFTPClientException(msg)
            raise ex

    def rmdir(self, url, completeCallback, arg, opAttr = None):
        """
        Remove a directory on a server.

        When the request is completed or aborted, the completeCallback
        will be invoked with the final status of the
        operation.

        The completeCallback must have the form:

        def completeCallback(arg, handle, error):
            - arg is the user argument passed in when the call was
              initiated
            - handle is the wrapper pointer to the client handle
            - error is None for success or a string if an error occurred

        @param url: the source URL for the directory to be removed
        @type url: string

        @param completeCallback: the function to be called
        when the mkdir operation is complete
        @type completeCallback: callable

        @param arg: user argument to pass to the callback function
        @type arg: any

        @param opAttr: an instance of OperationAttr for the source
        @type opAttr: instance of OperationAttr

        @return: None
        @rtype: None

        @raise GridFTPClientException: raised if unable to initiate the
        mkdir operation
        """

        if not opAttr:
            msg = "An OperationAttr instance must be input"
            ex = GridFTPClientException(msg)
            raise ex

        try:
            gridftpwrapper.gridftp_rmdir(self._handle, url, opAttr._attr, completeCallback, arg)
        except Exception, e:
            msg = "Unable to rmdir: %s" % e
            ex = GridFTPClientException(msg)
            raise ex

    def delete(self, url, completeCallback, arg, opAttr = None):
        """
        Delete a file on a server.

        When the request is completed or aborted, the completeCallback
        will be invoked with the final status of the
        operation.

        The completeCallback must have the form:

        def completeCallback(arg, handle, error):
            - arg is the user argument passed in when the call was
              initiated
            - handle is the wrapper pointer to the client handle
            - error is None for success or a string if an error occurred

        @param url: the source URL for the file to be deleted
        @type url: string

        @param completeCallback: the function to be called
        when the mkdir operation is complete
        @type completeCallback: callable

        @param arg: user argument to pass to the callback function
        @type arg: any

        @param opAttr: an instance of OperationAttr for the source
        @type opAttr: instance of OperationAttr

        @return: None
        @rtype: None

        @raise GridFTPClientException: raised if unable to initiate the
        delete operation
        """

        if not opAttr:
            msg = "An OperationAttr instance must be input"
            ex = GridFTPClientException(msg)
            raise ex

        try:
            gridftpwrapper.gridftp_delete(self._handle, url, opAttr._attr, completeCallback, arg)
        except Exception, e:
            msg = "Unable to delete: %s" % e
            ex = GridFTPClientException(msg)
            raise ex

    def move(self, src, dst, completeCallback, arg, opAttr = None):
        """
        Move or rename a file on a server.

        When the request is completed or aborted, the completeCallback
        will be invoked with the final status of the
        operation.

        The completeCallback must have the form:

        def completeCallback(arg, handle, error):
            - arg is the user argument passed in when the call was
              initiated
            - handle is the wrapper pointer to the client handle
            - error is None for success or a string if an error occurred

        @param src: the source URL for the file to be moved
        @type src: string

        @param dst: the destination URL for the file to be moved
        @type dst: string

        @param completeCallback: the function to be called
        when the mkdir operation is complete
        @type completeCallback: callable

        @param arg: user argument to pass to the callback function
        @type arg: any

        @param opAttr: an instance of OperationAttr for the source
        @type opAttr: instance of OperationAttr

        @return: None
        @rtype: None

        @raise GridFTPClientException: raised if unable to initiate the
        delete operation
        """

        if not opAttr:
            msg = "An OperationAttr instance must be input"
            ex = GridFTPClientException(msg)
            raise ex

        try:
            gridftpwrapper.gridftp_move(self._handle, src, dst, opAttr._attr, completeCallback, arg)
        except Exception, e:
            msg = "Unable to move: %s" % e
            ex = GridFTPClientException(msg)
            raise ex

    def chmod(self, url, mode, completeCallback, arg, opAttr = None):
        """
        Change the mode of a file on a server.

        When the request is completed or aborted, the completeCallback
        will be invoked with the final status of the
        operation.

        The completeCallback must have the form:

        def completeCallback(arg, handle, error):
            - arg is the user argument passed in when the call was
              initiated
            - handle is the wrapper pointer to the client handle
            - error is None for success or a string if an error occurred

        @param url: the source URL for the file 
        @type url: string

        @param mode: the integer file mode, be sure to use 4
        digits, for example 0644 instead of 644.
        @type mode: integer

        @param completeCallback: the function to be called
        when the mkdir operation is complete
        @type completeCallback: callable

        @param arg: user argument to pass to the callback function
        @type arg: any

        @param opAttr: an instance of OperationAttr for the source
        @type opAttr: instance of OperationAttr

        @return: None
        @rtype: None

        @raise GridFTPClientException: raised if unable to initiate the
        delete operation
        """

        if not opAttr:
            msg = "An OperationAttr instance must be input"
            ex = GridFTPClientException(msg)
            raise ex

        try:
            gridftpwrapper.gridftp_chmod(self._handle, url, mode, opAttr._attr, completeCallback, arg)
        except Exception, e:
            msg = "Unable to chmod: %s" % e
            ex = GridFTPClientException(msg)
            raise ex

    def verbose_list(self, url, completeCallback, arg, opAttr = None):
        """
        Get a file listing from a FTP server.

        This function starts a verbose file listing from an FTP server. If
        this function returns without exception then the user may immediately
        begin calling register_read() to retrieve the data associated with this
        operation.

        When all of the data associated with this URL is retrieved, and all
        of the data callbacks have been called, or if the verbose list request is
        aborted, the completeCallback will be invoked with the final
        status of the get.

        The completeCallback function must have the form:

        def completeCallback(arg, handle, error):
            - arg is the user argument passed in when the transfer was
              initiated
            - handle is the wrapped pointer to the client handle
            - error is None for success or a string if an error occurred

        @param url: the source URL from which to get the file listing
        @type url: string

        @param completeCallback: function to call when the listing is
        complete
        @type completeCallback: callable

        @param arg: user argument to pass to the callback
        @type arg: any

        @param opAttr: an instance of OperationAttr for the transfer
        @type opAttr: instance of OperationAttr

        @return: None
        @rtype: None

        @raise GridFTPClientException: raised if unable to initiate the verbose
        list operation

        """
        if not opAttr:
            msg = "An OperationAttr instance must be input"
            ex = GridFTPClientException(msg)
            raise ex

        try:
            gridftpwrapper.gridftp_verbose_list(
                self._handle, 
                url,
                opAttr._attr,
                completeCallback,
                arg
                )
        except Exception, e:
            msg = "Unable to initiate verbose list: %s" % e
            ex = GridFTPClientException(msg)
            raise ex

            
    def abort(self):
        """
        Abort the operation currently in progress.

        @return: None
        @rtype: None

        @raise GridFTPClientException: raised if unable to initiate the
        abort operation
        """

        if self._handle:
            try:
                gridftpwrapper.gridftp_abort(self._handle)
            except Exception, e:
                msg = "Unable to abort: %s" % e
                ex = GridFTPClientException(msg)
                raise ex

        
