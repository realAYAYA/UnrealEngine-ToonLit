const catchAsync = (callback: any) => (req: Request, res: Response, next: any) => {
    Promise.resolve(callback(req, res, next)).catch((error: any) => next(error));
};

module.exports = catchAsync;