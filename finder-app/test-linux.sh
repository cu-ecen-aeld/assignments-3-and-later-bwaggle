OUTDIR=/tmp/aeld

cd ${OUTDIR/rootfs}

TOOLCHAIN=$(aarch64-none-linux-gnu-gcc -print-sysroot)

echo "${TOOLCHAIN}/lib/ld-linux-aarch64.so.1"

sudo cp ${TOOLCHAIN}/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib
sudo cp ${TOOLCHAIN}/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64
sudo cp ${TOOLCHAIN}/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64
sudo cp ${TOOLCHAIN}/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64